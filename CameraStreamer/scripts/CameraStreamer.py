"""
    CameraStreamerClient is a class that connects to a camera streamer
    app to start/stop recording and start/stop cameras
"""

import logging
import socket
import json
import sys
import time
import traceback

#
# Creating basic logging mechanism
#
logging.basicConfig(level=logging.INFO,
                    format='[%(asctime)s] <%(name)s>: %(message)s',
                    )


DEFAULT_CS_SERVER_PORT = 6606


class CameraStreamerClient():
    """
     CameraStreamerClient controls a CameraStreamer client
    """
    def __init__(self, host, port=DEFAULT_CS_SERVER_PORT):
        self.logger = logging.getLogger('CameraStreamer(%s:%d)' % (host, port))
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket_connected = False

        # socket settings
        self.host = host
        self.port = port

        # reading loop
        self.message = []
        self.bytes_read = 0
        self.next_msg_len = 0

        # camera status
        self._camera_struct_last = int(time.time() * 1000)
        self._camera_struct = {'capturing': False, 'captureDeviceUserDefinedName': '', 'captureDeviceType': 'opencv',
                               'captureDeviceSerial': 'opencv::webcam::index=0', 'capturingDepth': False,
                               'capturingColor': False, 'captureDepthWidth': 0, 'captureDepthHeight': 0,
                               'captureColorWidth': 0, 'captureColorHeight': 0,
                               'streaming': False, 'streamingClients': 0,
                               'streamingMaxFPS': 0,
                               'streamingCameraParameters': '{"camera_matrix": [[0, 0.0,0],[0.0, 0, 0],[0.0, 0.0, 1.0]],"dist_coeff" : [[0, 0, 0, 0, 0, 0, 0, 0]],  "mean_error" : 0.00}',
                               'streamingColor': False, 'streamingColorWidth': 0, 'streamingColorHeight': 0,
                               'streamingColorFormat': '', 'streamingColorBitrate': 0.0,
                               'streamingDepth': False, 'streamingDepthWidth': 0, 'streamingDepthHeight': 0,
                               'streamingDepthFormat': '', 'streamingDepthBitrate': 0.0, 'recording': False,
                               'recordingColor': False, 'recordingDepth': False, 'recordingDepthPath': '',
                               'recordingColorPath': '', 'port': 0, 'controlPort': 0}

        # callbacks
        self.on_connect = None
        self.on_disconnect = None
        self.on_pong = None

        # maps a message to a handler
        self.msg_handler_map = {
            "pong": self._handle_pong_message,
        }

    def _handle_pong_message(self, msg):
        self._camera_struct_last = int(time.time() * 1000)
        self._camera_struct = msg

    def connect(self, timeout=30):
        if self._socket_connected:
            return True

        if timeout is None:
            timeout = 3120
        self.logger.info("Connecting to CameraStreamer")
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # connects to end-point
        try:
            self._socket_connected = False
            self._socket.settimeout(timeout)
            self._socket.connect((self.host, self.port))
            self._socket_connected = True
        except ConnectionRefusedError as e:
            self.logger.info("Unable to connect. Is the server running?")
        except TimeoutError as te:
            self.logger.info("Timed out (30 seconds).")
        except ConnectionResetError as ere:
            self.logger.error("Connection reset by remote host: %s. Is the server running?", ere)
        except ConnectionAbortedError as eae:
            self.logger.error("Connection aborted: %s", eae)
        except OSError as e:
            self.logger.error(e)
        except:
            self.logger.error("Unhandled exception: %s %s <----------------------", sys.exc_info()[0],
                              sys.exc_info()[1])
            traceback.print_exc()

        # reads messages
        if self._socket_connected:
            self.logger.info("Connected")
            self._socket.settimeout(None)

        return self._socket_connected

    def disconnect(self):
        if self.is_connected():
            self._socket.close()

    def _ping(self):
        '''internal call for a status update from camerastreamer. returns false if something went wrong'''
        if self.is_connected():
            # only requests for a status update if we haven't asked for one in
            # 500 ms
            if int(time.time() * 1000) - self._camera_struct_last <= 500:
                return True

            # asks for a status update
            self.send(json.dumps({"type": "ping"}).encode())

            # waits for the update
            self.read_and_handle_response()

            return True

        return False

    def is_connected(self):
        """returns true if connected to a camera streamer server"""
        return self._socket_connected

    def is_camera_running(self):
        """returns true if the camera is running"""
        if self._ping():
            return self._camera_struct['streaming']
        return False

    def is_recording(self):
        if self._ping():
            return self._camera_struct['recording']
        return False

    def get_color_filename(self):
        if self._ping():
            return self._camera_struct['recordingColorPath']
        return None

    def get_depth_filename(self):
        if self._ping():
            return self._camera_struct['recordingDepthPath']
        return None

    def start_recording(self, colorPath, colorFilename="", depthFileName="", depthPath="", recordColor=True,
                        recordDepth=False):
        if self.is_connected():
            return self.send(json.dumps({"type": "startRecording", "color": recordColor, "depth": recordDepth,
                                         "colorPath": colorPath, "depthPath": depthPath, "colorFilename": colorFilename,
                                         "depthFilename" : depthFileName}).encode())
        return False

    def stop_recording(self):
        if self.is_connected():
            return self.send(json.dumps({"type": "stopRecording"}).encode())
        return False

    def start_camera(self):
        if self.is_connected():
            return self.send(json.dumps({"type": "startCamera"}).encode())
        return False

    def stop_camera(self):
        if self.is_connected():
            return self.send(json.dumps({"type": "stopCamera"}).encode())
        return False

    def disconnect(self):
        if self.is_connected():
            self._socket.close()
            self._socket_connected = False

    def send(self, message):
        ''' sends a message to the clients. Expects a byte array'''
        if self.is_connected():
            try:
                message_len = len(message)
                self._socket.send(message_len.to_bytes(4, 'little'))
                self._socket.send(message)
                self.logger.debug("%s", message)
            except ConnectionResetError as e:
                self.logger.error("Connection reset by remote host while sending message: %s", e)
                return False
            except ConnectionAbortedError as e:
                self.logger.error("Connection aborted while sending message: %s", e)
                return False
            except OSError as e:
                self.logger.error(e)
                return False
            except:
                self.logger.error("Unhandled exception: %s %s <----------------------", sys.exc_info()[0],
                                  sys.exc_info()[1])
                traceback.print_exc()
                return True

    def read_and_handle_response(self):
        # makes sure that clients have to wait for an answer
        if self._socket_connected:
            try:
                # self.request is the TCP socket connected
                self.message = []
                self.bytes_read = 0
                self.next_msg_len = self._socket.recv(4)

                if len(self.next_msg_len) > 0:
                    self.next_msg_len = int.from_bytes(self.next_msg_len, "little")

                    # read the message
                    while self.bytes_read < self.next_msg_len:
                        byte_buffer = self._socket.recv(self.next_msg_len)
                        if len(byte_buffer) == 0:
                            self.logger.error("Incomplete message received (%d out of %d bytes)"
                                              % (self.bytes_read, self.next_msg_len))
                        self.bytes_read += len(byte_buffer)
                        self.message.append(byte_buffer)

                    # done reading full message with everything we need?
                    if self.bytes_read == self.next_msg_len:
                        self.message = b''.join(self.message)
                        self.handle_message(self.message)

            except ConnectionResetError as cre:
                self.logger.error("Connection reset by remote host: %s", cre)
                self._socket_connected = False
            except ConnectionAbortedError as cae:
                self.logger.error("Connection aborted: %s", cae)
                self._socket_connected = False
            except OSError as oe:
                self.logger.error(oe)
                self._socket_connected = False
            except RuntimeError as re:
                self.logger.error(re)
                self._socket_connected = False
            except:
                self.logger.error("Unhandled exception: %s %s <----------------------", sys.exc_info()[0],
                                  sys.exc_info()[1])
                traceback.print_exc()
                self._socket_connected = False

    def handle_message(self, message):
        try:
            msg_json = json.loads(message.decode("utf-8"))

            if msg_json["type"] in self.msg_handler_map:
                self.msg_handler_map[msg_json["type"]](msg_json)
            else:
                self.logger.warning("Invalid  message type %s", msg_json["type"])

        except json.JSONDecodeError as jde:
            self.logger.error("Invalid json received: %s", jde.msg)
        except:
            self.logger.error("Unhandled exception: %s %s <----------------------", sys.exc_info()[0],
                              sys.exc_info()[1])
            traceback.print_exc()
