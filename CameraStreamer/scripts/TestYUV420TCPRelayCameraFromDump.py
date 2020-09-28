'''
  This script reads a network dump file and sends it over to the first connected client
  the network dump contains YUV420p frames but also their resolution in the following format:
  <total packet length: 4 bytes><frame width: 4 bytes><frame height: 4 bytes> <yuv420p frame>

'''

import socket
import sys
import os
import numpy as np
import cv2
import math
import time


# ------------------------------------------------------------------------------------------ PARAMETERS
# do we have any parameters?
if len(sys.argv) <= 2:
  print("Usage:\n\t %s <filename.bin> [port (optional - defaults to 40123)]" % sys.argv[0])
  exit(1)

FILENAME = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) >= 4 else 40123

# ------------------------------------------------------------------------------------------ OPENING FILE / CACULATING FRAMES

# Opens file
print("Opening file %s ..." % FILENAME)
f = open(FILENAME, "rb")

packetSize = int.from_bytes(f.read(4), "little")
WIDTH = int.from_bytes(f.read(4), "little")
HEIGHT = int.from_bytes(f.read(4), "little")
expectedFrameSize = int(WIDTH * HEIGHT * 1.5)
print("File with resolution of %dx%d" % (WIDTH,HEIGHT))
f.close()

fileSize = os.path.getsize(FILENAME)
expectedFrameSize = int(WIDTH * HEIGHT * 1.5)
frameSize = packetSize - 8

if (frameSize != expectedFrameSize):
    print("Error! Expecting a frameSize of %d but found %d" % (expectedFrameSize, frameSize))
    # we will ignore frameSize and use expectedFrameSize
    frameSize = expectedFrameSize

# we need to 8 bytes to frameSize to make sure its accounting for the width, height header
frameSize += 8

frameCount = math.floor(fileSize / (frameSize + 4))

print("File has %d mb - %d frames or %f minutes (at 30fps)" % (fileSize/(1024*1024), frameCount, frameCount/(30*60)))

# ------------------------------------------------------------------------------------------ CREATING SOCKET

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_address = ('0.0.0.0', PORT)
print("Listening at %d" % PORT)

# Listen for incoming connections
sock.bind(server_address)
sock.listen(1)

# ------------------------------------------------------------------------------------------ HELPER FUNCTION TO SEND FRAME (todo: modularize)
# helper function to write an entire chunck of data
def sendEntireMessage(sock, message):
    totalsent = 0
    while totalsent < len(message):
        sent = sock.send(message[totalsent:])
        if sent == 0:
            raise RuntimeError("socket connection broken")
        totalsent = totalsent + sent
    return len(message)

# ------------------------------------------------------------------------------------------ STREAM LOOP - only one client at a time

while True:
    # Wait for a connection
    print("Waiting for a connection...")
    frameCount = 0
    connection, client_address = sock.accept()
    print("New client connected - ", client_address)
    #time.sleep(2)
    # opens the file
    try:
        with open(FILENAME, "rb") as f:
            # read as many bytes as necessary for the yuv frame
            int.from_bytes(f.read(4), "little") # ignore length header
            length = frameSize.to_bytes(4, "little")
            wh_yuvframe = f.read(frameSize)
            while wh_yuvframe:
                if len(wh_yuvframe) < frameSize:
                    print("Invalid frame size found... restarting...")
                    break
                
                sendEntireMessage(connection, length)
                sendEntireMessage(connection, wh_yuvframe)
                frameCount += 1

                # read from the file
                tmp = int.from_bytes(f.read(4), "little")
                if tmp:
                    wh_yuvframe = f.read(frameSize)
                else:
                    wh_yuvframe = None
                
                time.sleep(0.016)
            # done reading the file
            print("Streamed the file (%d frames).. repeating after a reconnect...", frameCount)
    except ConnectionAbortedError:
        print("Client disconnected after streaming %d frames: " % frameCount)
    
    # closes connection
    connection.close()

    
