import socket
import cv2
import numpy

PORT = 3614
ADDR = "localhost"

# sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# sock.connect((ADDR, PORT))
# numStreams = int.from_bytes(sock.recv(4), "big")
# print("Connecting to", numStreams, "streams")

stream = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
stream.connect((ADDR, PORT))
print("Successfully connected to stream on port", (PORT))


def receivedEntireMessage(sock, length):
  chunks = []
  bytes_recd = 0
  while bytes_recd < length:
      chunk = sock.recv(length - bytes_recd)
      if chunk == b'':
          raise RuntimeError("socket connection broken")
      chunks.append(chunk)
      bytes_recd = bytes_recd + len(chunk)
  return b''.join(chunks)


while True:
  # read header
  width = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  height = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  colorLen = int.from_bytes(receivedEntireMessage(stream, 4), "little") # 0 if no color frame
  depthLen = int.from_bytes(receivedEntireMessage(stream, 4), "little") # 0 if no depth frame
  
  print("Got frame with (w=%d,h=%d,color=%d,depth=%d)" % (width, height, colorLen, depthLen))

  # read color if enabled
  if (colorLen > 0):
    colorData = receivedEntireMessage(stream, colorLen)

  # read depth if enabled
  if (depthLen > 0):
    depthData = receivedEntireMessage(stream, depthLen)

  # show color if enabled
  if (colorLen > 0):
    numpyarr = numpy.fromstring(colorData, numpy.uint8)
    frame = cv2.imdecode(numpyarr, cv2.IMREAD_COLOR)
    cv2.imshow('Color', frame)

  # show depth if enabled
  if (depthLen > 0):
    deptharray = numpy.fromstring(depthData, numpy.uint16).reshape(height,width)
    cv2.imshow('Depth', cv2.normalize(deptharray, dst=None, alpha=0, beta=65535, norm_type=cv2.NORM_MINMAX))

  if (cv2.waitKey(1) & 0xFF == 27):
    break

stream.close()
