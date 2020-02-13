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
  # Get initial uint32 with frame size
  width = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  height = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  colorLen = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  depthLen = int.from_bytes(receivedEntireMessage(stream, 4), "little")
  
  print("Got frame with (w=%d,h=%d,color=%d,depth=%d)" % (width, height, colorLen, depthLen))
  
  colorData = receivedEntireMessage(stream, colorLen)
  depthData = receivedEntireMessage(stream, depthLen)

  numpyarr = numpy.fromstring(colorData, numpy.uint8)
  #print(numpyarr.shape)
  frame = cv2.imdecode(numpyarr, cv2.IMREAD_COLOR)
  #print(frame)
  
  cv2.imshow('Color', frame)

  deptharray = numpy.fromstring(depthData, numpy.uint16).reshape(height,width)
  cv2.imshow('Depth', cv2.normalize(deptharray, dst=None, alpha=0, beta=65535, norm_type=cv2.NORM_MINMAX))
  if (cv2.waitKey(1) & 0xFF == 27):
    break

stream.close()
