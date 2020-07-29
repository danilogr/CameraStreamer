import socket
import sys
import os
import numpy as np
import cv2
import math
import time


# ------------------------------------------------------------------------------------------ PARAMETERS
# do we have any parameters?
if len(sys.argv) <= 3:
  print("Usage:\n\t %s <filename.yuv YUV420> <width> <height> [port (optional - defaults to 51234)]" % sys.argv[0])
  exit(1)

FILENAME = sys.argv[1]
WIDTH = int(sys.argv[2])
HEIGHT = int(sys.argv[3])

if (WIDTH % 4 != 0):
    print("Invalid width %d! (It should be divisible by 4)"%WIDTH)

if (HEIGHT % 4 != 0):
    print("Invalid height %d! (It should be divisible by 4)"%HEIGHT)

PORT = int(sys.argv[4]) if len(sys.argv) >= 4 else 40123

# ------------------------------------------------------------------------------------------ OPENING FILE / CACULATING FRAMES

# Opens file
print("Opening file %s ..." % FILENAME)
fileSize = os.path.getsize(FILENAME)
frameSize = int(WIDTH * HEIGHT * 1.5)
frameCount = math.floor(fileSize / frameSize)

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

# ------------------------------------------------------------------------------------------ CREATE HEADER


frameSizeEnc = (frameSize + 8).to_bytes(4, "little") # frameSize + 4 bytes for height + 4 bytes for width
heightEncoded = HEIGHT.to_bytes(4, "little")
widthEncoded = WIDTH.to_bytes(4, "little")
messageHeader = b"".join([frameSizeEnc, widthEncoded, heightEncoded])


# ------------------------------------------------------------------------------------------ STREAM LOOP - only one client at a time

while True:
    # Wait for a connection
    print("Waiting for a connection...")
    connection, client_address = sock.accept()
    print("New client connected - ", client_address)
    #time.sleep(2)
    # opens the file
    with open(FILENAME, "rb") as f:
        # read as many bytes as necessary for the yuv frame
        yuvframe = f.read(frameSize)
        while yuvframe:
            # write header
            sendEntireMessage(connection, messageHeader)

            # write the amount read
            sendEntireMessage(connection, yuvframe)

            # read from the file
            yuvframe = f.read(frameSize)
            time.sleep(0.01)
        # done reading the file
        print("Streamed the file.. repeating after a reconnect...")
    
    # closes connection
    connection.close()

    
