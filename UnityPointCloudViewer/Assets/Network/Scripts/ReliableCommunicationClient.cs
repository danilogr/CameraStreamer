/*

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using UnityEditor;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.UI;

/// <summary>
/// Each connection is represented by a ReliableCommunicationClient
/// 
/// Each client runs its thread and has unique events
/// </summary>
public class ReliableCommunicationClient
{
    public Thread clientReadLoop;

    // dropping packets?
    [Tooltip("Data events will be called only with the last message received. Use wisely")]
    public bool dropAccumulatedMessages = false;

    bool killThreadRequested = true;
    public Queue<JSONObject> messageQueue = new Queue<JSONObject>();
    System.Object messageQueueLock = new System.Object();

    TcpClient tcpClient;
    string LogName; // TODO

    // flags for classes managing this client (e.g.: ReliableCommunicationServer)
    bool onDisconnectRaised = false;
    bool socketConnected = false;



    ReliableCommunicationClient(string clientName, TcpClient connection)
    {

        if (!connection.Connected)
        {
            // todo, throw
        }

        socketConnected = true;

        try
        {
            clientReadLoop = new Thread(new ThreadStart(ClientLoopThread));
            clientReadLoop.IsBackground = true;
            clientReadLoop.Start();
        }
        catch (Exception e)
        {
            Debug.LogError(LogName + " Failed to start socket thread: " + e);
            
        }
    }

    public void Disconnect()
    {
        // Stop Thread if it is still running
        if (clientReadLoop != null && clientReadLoop.IsAlive)
        {

            try
            {
                clientReadLoop.Join(WaitToAbortMs);
                if (listenerThread.IsAlive)
                    listenerThread.Abort();

                listenerThread = null;
            }
            catch (Exception)
            {
                // don't care
            }

        }

        // Is it connected? then let others know that this socket is not connected anymore
        if (socketConnected)
        {
            socketConnected = false;
            _onStoppedListening = false; // we are not sure that there will be another update loop

            // disconnect all clients and invoke their final events
            // TODO

            // make sure others are aware that this socket disconnected
            if (OnStopListening != null)
                OnStopListening.Invoke(this);
        }
    }

    public string RemoteEndpointIP()
    {
        if (tcpClient != null)
        {
            return ((IPEndPoint)tcpClient.Client.RemoteEndPoint).Address.ToString();
        }

        return "";
    }

    public int RemoteEndpointPort()
    {
        if (tcpClient != null)
        {
            return ((IPEndPoint)tcpClient.Client.RemoteEndPoint).Port;
        }

        return -1;
    }

    /// <summary>
    /// This thread should be instantiated with an ongoing connection
    /// </summary>
    private void ClientLoopThread()
    {
        byte[] lengthHeader = new byte[4];
        socketConnected = true;
        while (!killThreadRequested)
        {
            try
            {
                Debug.Log(String.Format("{0} New connection from {1}:{2}", LogName, RemoteEndpointIP(), RemoteEndpointPort()));
                //                    statisticsReporter.RecordConnectionEstablished();


                // handles messages
                using (NetworkStream stream = tcpClient.GetStream())
                {
                    //statisticsReporter.RecordConnectionEstablished();
                    try
                    {
                        while (!killThreadRequested)
                        {
                            // reads 4 bytes - header
                            readToBuffer(stream, lengthHeader, lengthHeader.Length);

                            // convert to int (UInt32LE)
                            UInt32 msgLength = BitConverter.ToUInt32(lengthHeader, 0);

                            // create appropriately sized byte array for message
                            byte[] bytes = new byte[msgLength];

                            // create appropriately sized byte array for message
                            bytes = new byte[msgLength];
                            readToBuffer(stream, bytes, bytes.Length);

                            // parse to json
                            try
                            {
                                string msgString = Encoding.UTF8.GetString(bytes);
                                JSONObject msgJson = new JSONObject(msgString);

                                //statisticsReporter.RecordMessageReceived();
                                lock (messageQueueLock)
                                {
                                    messageQueue.Enqueue(msgJson);
                                }

                            }
                            catch (Exception err)
                            {
                                Debug.LogError(LogName + "Error parsing message (JSON) -> " + err.ToString());
                            }
                        }
                    }
                    catch (System.IO.IOException ioException)
                    {
                        // when stream read fails, it throws IOException.
                        // let's expose that exception and handle it below
                        throw ioException.InnerException;
                    }
                }
            }
            catch (SocketException socketException)
            {
                switch (socketException.SocketErrorCode)
                {
                    case SocketError.Interrupted:
                        return; // we were forcefully canceled - free thread
                    case SocketError.Shutdown:
                        return; // we forcefully freed the socket, so yeah, we will get an error
                    case SocketError.TimedOut:
                        Debug.LogError(LogName + "timed out");
                        //                            statisticsReporter.RecordStreamError();
                        break;
                    case SocketError.ConnectionRefused:
                        if (killThreadRequested)
                            Debug.LogError(LogName + "connection refused! Are you sure the server is running?");
                        else
                            Debug.LogError(LogName + "connection refused! Are you sure the server is running? - Trying again in " + (ReconnectTimeoutMs / 1000f) + " sec");
                        //                            statisticsReporter.RecordStreamError();
                        break;
                    case SocketError.NotConnected:
                        // this sounds extra, but sockets that never connected will die with NotConnected
                        if (socketConnected)
                        {
                            Debug.LogError(LogName + " Socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                            //                                statisticsReporter.RecordStreamError();
                        }
                        break;
                    default:
                        // if we didn't interrupt it -> reconnect, report statistics, log warning
                        Debug.LogError(LogName + " Socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                        //                            statisticsReporter.RecordStreamError();
                        break;
                }

            }
            catch (ObjectDisposedException)
            {
                // this exception happens when the socket could not finish  its operation
                // and we forcefully aborted the thread and cleared the object
            }
            catch (ThreadAbortException)
            {
                // this exception happens when the socket could not finish  its operation
                // and we forcefully aborted the thread (we wait 100 ms)
            }
            catch (SocketDisconnected)
            {
                // this is our very own exception for when a client disconnects during a read
                // do nothing.. finally will take care of it below
            }
            catch (Exception e)
            {
                // this is likely not a socket error. So while we do not record a stream error,
                // we still log for later learning about it
                Debug.LogWarning(LogName + "Exception " + e);
            }
            finally
            {
                if (socketConnected)
                {
                    Debug.Log(LogName + "Disconnected");
                    //                        statisticsReporter.RecordStreamDisconnect();
                    onDisconnectRaised = true;
                }
            }
        }
    }

    /// <summary>
    /// Reads readLength bytes from a network stream and saves it to buffer
    /// </summary>
    /// <param name="buffer"></param>
    /// <param name="readLength"></param>
    void readToBuffer(NetworkStream stream, byte[] buffer, int readLength)
    {
        int offset = 0;
        // keeps reading until a full message is received
        while (offset < buffer.Length)
        {
            int bytesRead = stream.Read(buffer, offset, readLength - offset); // read from stream
                                                                              //statisticsReporter.RecordPacketReceived(bytesRead);

            // "  If the remote host shuts down the connection, and all available data has been received,
            // the Read method completes immediately and return zero bytes. "
            // https://docs.microsoft.com/en-us/dotnet/api/system.net.sockets.networkstream.read?view=netframework-4.0
            if (bytesRead == 0)
            {
                throw new SocketDisconnected();// returning here means that we are done
            }

            offset += bytesRead; // updates offset
        }
    }

    /// <summary>
    /// We raise this exception when the socket disconnects mid-read
    /// </summary>
    public class SocketDisconnected : Exception
    {
    }

    public void Send(JSONObject msg)
    {
        Send(msg.ToString());
    }

    public void Send(string msg)
    {
        byte[] msgAsBytes = Encoding.UTF8.GetBytes(msg);
        Send(msgAsBytes);
    }

    public void Send(byte[] msg)
    {
        if (tcpClient == null)
        {
            Debug.LogWarning(LogName + "not connected! Dropping message...");
            return;
        }

        // Build message with Headers
        byte[] bytesToSend;

        byte[] messageLength = BitConverter.GetBytes((UInt32)msg.Length);
        bytesToSend = new byte[msg.Length + sizeof(UInt32)];

        // header
        Buffer.BlockCopy(messageLength, 0, bytesToSend, 0, sizeof(UInt32));

        // payload
        Buffer.BlockCopy(msg, 0, bytesToSend, sizeof(UInt32), msg.Length);


        // Send Message
        try
        {
            // Get a stream object for writing.
            NetworkStream stream = tcpClient.GetStream();

            // Send
            if (stream.CanWrite)
            {
                stream.Write(bytesToSend, 0, bytesToSend.Length);
                //statisticsReporter.RecordMessageSent(bytesToSend.Length);
            }
        }
        catch (SocketException e)
        {
            Debug.Log(LogName + " Socket Exception while sending: " + e);
        }
    }


}

*/