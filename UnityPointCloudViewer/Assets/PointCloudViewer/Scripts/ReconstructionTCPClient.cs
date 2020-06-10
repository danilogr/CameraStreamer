using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using UnityEngine;
using UnityEngine.Events;



[Serializable]
/// Callback that receives width, height, color buffer, depth buffer
public class MeshReadyEvent : UnityEvent<int, int, byte[], byte[]> { }

public class ReconstructionTCPClient : MonoBehaviour
{

    // members related to invoking another piece of code when something happens (I shall change this to
    // share a texture)
    #region
    public MeshReadyEvent MeshReady;
    public bool TestLocalNetwork;
    #endregion

    #region public members

    //public PcxReconstructionReceiver ReconstructionReceiver;


    [Tooltip("Dynamic fusion server ip")]
    public string HostIp;

    [Tooltip("Dynamic fusion server port")]
    public int HostPort = 27015;

    bool killThreadRequested = false;

    public bool dropQueuedFrames = true;



    [HideInInspector]
    public bool FreezeCanonical = false;


    [HideInInspector]
    public Queue<Tuple<int, int, byte[], byte[]>> bufferQueue = new Queue<Tuple<int, int, byte[], byte[]>>();
    #endregion

    #region private members
    private TcpClient socketConnection;
    private Thread clientReceiveThread;
    private CommunicationStatistics statisticsReporter;
    private string LogName;

    // libjpeg decoder
    IntPtr decoderHandle;
    bool decoderAvailabe = false;
    const int decodingFlags = LibJPEGTurbo.TJFLAG_ACCURATEDCT | LibJPEGTurbo.TJFLAG_BOTTOMUP;
    #endregion


    /// <summary>
    /// Returns true if the video stream was already decoded to RGB
    /// </summary>
    /// <returns></returns>
    public bool isFastDecoderAvailable()
    {
        return decoderAvailabe;
    }


    void Awake()
    {

        LogName = "[ReconstructionTCPClient] - ";
        if (TestLocalNetwork)
            HostIp = "127.0.0.1";

        IPAddress ip = IPAddress.Parse(HostIp);

        // making sure sockets report statistics regardless of how they were instantiated
        statisticsReporter = GetComponent<CommunicationStatistics>();
        if (statisticsReporter == null)
        {
            statisticsReporter = this.gameObject.AddComponent<CommunicationStatistics>();
            statisticsReporter.Name = "ReconstructionTCPClient";
            statisticsReporter.TCP = true;
        }
    }

    // Connects to the server when enabled
    void OnEnable()
    {
        // tries to allocate decoder
        decoderAvailabe = true;
        try
        {
            decoderHandle = LibJPEGTurbo.tjInitDecompress();
            if (decoderHandle == IntPtr.Zero)
            {
                // we found the library, but LibJPEGTurbo failed
                Debug.LogError(string.Format("{0}Could not start LibJPEGTurbo (Internal Error)! Using Unity's built-in decoder (slow)...!", LogName));
                decoderAvailabe = false;
            }
        }
        catch (Exception e)
        {
            Debug.LogError(string.Format("{0}Could not start LibJPEGTurbo! Using Unity's built-in decoder (slow)...!{1}", LogName, e));
            decoderAvailabe = false;
        }


        killThreadRequested = false;
        ConnectToTcpServer();
    }

    // Disconnects from the server when disabled
    private void OnDisable()
    {
        CloseConnection();
        LogName = "[ReconstructionTCPClient] - ";

        // frees decoders and buffers
        if (decoderHandle != null && decoderHandle != IntPtr.Zero)
        {
            LibJPEGTurbo.tjDestroy(decoderHandle);
            decoderHandle = IntPtr.Zero;
        }
    }

    // Update is called once per frame
    long packetsReceived = 0, packetsDropped = 0;

    void Update()
    {
        if (bufferQueue.Count > 0)
        {
            Queue<Tuple<int, int, byte[], byte[]>> tmpQueue = bufferQueue;

            // changes the bufferQueue to an empty one so that the decoding thread can keep handling it
            lock (bufferQueue)
            {
                bufferQueue = new Queue<Tuple<int, int, byte[], byte[]>>();
            }

            // now we can do whatever we want with tmpQueue
            if (dropQueuedFrames)
            {
                packetsDropped += tmpQueue.Count - 1;
                while (tmpQueue.Count > 1)
                    tmpQueue.Dequeue();
            }

            Tuple<int, int, byte[], byte[]> t = tmpQueue.Dequeue();
            // invokes callback
            if (MeshReady != null)
            {
                // width, height, color buffer, depth buffer
                MeshReady.Invoke(t.Item1, t.Item2, t.Item3, t.Item4);
            }

            t = null;
        }

    }


    /// <summary>
    /// Setup socket connection.
    /// </summary>
    private void ConnectToTcpServer()
    {
        try
        {
            if (clientReceiveThread == null || !clientReceiveThread.IsAlive)
            {
                clientReceiveThread = new Thread(new ThreadStart(ListenForData));
                clientReceiveThread.IsBackground = true;
                clientReceiveThread.Start();
            }
            else
            {
                Debug.LogWarning(LogName + "Already connected. Disconnect before connecting!");
            }
        }
        catch (Exception e)
        {
            Debug.Log(LogName + "On client connect exception " + e);
        }
    }
    /// <summary>
    /// Runs in background clientReceiveThread; Listens for incomming data (color + depth) from CV server
    /// </summary>
    private void ListenForData()
    {
        bool firstTime = true;
        bool connected = false;
        DateTime connectionStarted = DateTime.Now;
        // conncets and reads ad infinitum
        while (!killThreadRequested)
        {
            try
            {
                if (!firstTime)
                {
                    Thread.Sleep(1000); // waits a second before trying to connect again
                }
                else
                {
                    firstTime = false;
                }

                Debug.Log(LogName + "Connecting to " + HostIp + ":" + HostPort);

                connected = false;
                // connects to the server
                socketConnection = new TcpClient(HostIp, HostPort);
                connected = true;

                Debug.Log(LogName + "Connected to " + HostIp);
                statisticsReporter.RecordConnectionEstablished();
                connectionStarted = DateTime.Now;

                using (NetworkStream stream = socketConnection.GetStream())
                {
                    // header of data receiving from server - length of image and depth data
                    Byte[] widthLengthHeader = new byte[4];
                    Byte[] heightLengthHeader = new byte[4];
                    Byte[] rgbLengthHeader = new byte[4];
                    Byte[] depthLengthHeader = new byte[4];

                    // starts statistics
                    packetsDropped = 0;
                    packetsReceived = 0;

                    // Get a stream object for reading
                    while (!killThreadRequested)
                    {
                        // Testing the Color Receiving

                        // expected received stream - width + height+ rbgsize + depthsize + rgbdata + depthdata
                        // TODO: Fix each one of these reads.
                        //int offset = 0;
                        //while (offset < widthLengthHeader.Length)
                        //
                        stream.Read(widthLengthHeader, 0, widthLengthHeader.Length);     // read width value at 0
                        stream.Read(heightLengthHeader, 0, heightLengthHeader.Length);   // read height value at 0
                        stream.Read(rgbLengthHeader, 0, rgbLengthHeader.Length);         // read rbgsize value at 0
                        stream.Read(depthLengthHeader, 0, depthLengthHeader.Length);     // read depthsize value at 0

                        // convert to int (UInt32LE)
                        UInt32 width = BitConverter.ToUInt32(widthLengthHeader, 0);
                        UInt32 height = BitConverter.ToUInt32(heightLengthHeader, 0);
                        UInt32 rgb_length = BitConverter.ToUInt32(rgbLengthHeader, 0);
                        UInt32 depth_length = BitConverter.ToUInt32(depthLengthHeader, 0);

                        // var ImageWidth = Convert.ToInt32(width);
                        // var ImageHeight = Convert.ToInt32(height);
                        //Debug.Log("" + width + "x" + height + " RGB Length: " + rgb_length + "   Depth Length: " + depth_length);

                        Byte[] rgbbytes = new byte[rgb_length];
                        Byte[] depthbytes = new byte[depth_length];

                        // keeps reading until a full message is received
                        // reading rgb data
                        int offset = 0;
                        while (offset < rgbbytes.Length)
                        {
                            //Debug.Log("Getting RGB Bytes");
                            int bytesRead = stream.Read(rgbbytes, offset, rgbbytes.Length - offset); // read from stream

                            //string s = System.Text.Encoding.UTF8.GetString(socketBufferV3, 0, socketBufferV3.Length);
                            //print(s);

                            // "  If the remote host shuts down the connection, and all available data has been received,
                            // the Read method completes immediately and return zero bytes. "
                            // https://docs.microsoft.com/en-us/dotnet/api/system.net.sockets.networkstream.read?view=netframework-4.0
                            if (bytesRead == 0)
                            {
                                // we were disconnected and it is very likely that an exception was thrown; if not, let's throw it and try to reconnect (outer while loop)
                                throw new SocketException(995); // WSA_OPERATION_ABORTED
                            }

                            offset += bytesRead; // updates offset

                        }


                        // reading depth data
                        offset = 0; // reset offset
                        while (offset < depthbytes.Length)
                        {
                            //Debug.Log("Getting Depth Bytes");
                            int bytesRead = stream.Read(depthbytes, offset, depthbytes.Length - offset); // read from stream

                            if (bytesRead == 0)
                            {
                                // we were disconnected and it is very likely that an exception was thrown; if not, let's throw it and try to reconnect (outer while loop)
                                throw new SocketException(995); // WSA_OPERATION_ABORTED
                            }

                            offset += bytesRead; // updates offset

                        }

                        //ushort[] depthData = new UInt16[depthbytes.Length];
                        //Buffer.BlockCopy(depthbytes, 0, depthData, 0, depthbytes.Length);
                        ++packetsReceived;

                        if (!FreezeCanonical)
                        {

                            byte[] decodedJpegBytes;

                            // decodes the jpeg if a decoder is available
                            if (decoderAvailabe)
                            {
                                // while we have frames, decode them
                                int jpegWidth = 0, jpegHeight = 0, jpegSubsample = 0;
                                if (LibJPEGTurbo.tjDecompressHeader2(decoderHandle, rgbbytes, rgbbytes.LongLength, ref jpegWidth, ref jpegHeight, ref jpegSubsample) == 0)
                                {
                                    decodedJpegBytes = new byte[jpegWidth * jpegHeight * 3];

                                    if (LibJPEGTurbo.tjDecompress2(decoderHandle, rgbbytes, rgbbytes.LongLength, decodedJpegBytes, jpegWidth, 0, jpegHeight, (int)LibJPEGTurbo.TJPF.TJPF_RGB, decodingFlags) == 0)
                                    {
                                        // increment decoder counter
                                        //++decodedFrames;
                                        lock (bufferQueue)
                                        {
                                            bufferQueue.Enqueue(new Tuple<int, int, byte[], byte[]>((int)width, (int)height, depthbytes, decodedJpegBytes));
                                        }
                                    }
                                }
                            }
                            else
                            {

                                lock (bufferQueue)
                                {
                                    bufferQueue.Enqueue(new Tuple<int, int, byte[], byte[]>((int)width, (int)height, depthbytes, rgbbytes));
                                }
                            }
                        }

                    }
                }
            }
            catch (SocketException socketException)
            {
                switch (socketException.SocketErrorCode)
                {
                    case SocketError.Interrupted:
                        return; // we were forcefully canceled - free thread
                    case SocketError.TimedOut:
                        if (killThreadRequested)
                            Debug.LogError(LogName + "timed out");
                        else
                            Debug.LogError(LogName + "timed out - Trying again in 1 sec");
                        statisticsReporter.RecordStreamError();
                        break;
                    case SocketError.NotConnected:
                        // this sounds extra, but sockets that never connected will die with NotConnected
                        if (connected)
                        {
                            Debug.LogError(LogName + " Socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                            statisticsReporter.RecordStreamError();
                        }
                        break;
                    default:
                        // if we didn't interrupt it -> reconnect, report statistics, log warning
                        Debug.LogError(LogName + " Socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                        statisticsReporter.RecordStreamError();
                        break;
                }
            }
            catch (ThreadAbortException e)
            {
                // this exception happens when the socket could not finish  its operation
            }
            catch (Exception e)
            {
                // this is likely not a socket error. So while we do not record a stream error,
                // we still log for later learning about it
                Debug.LogWarning(LogName + "Exception " + e);
            }
            finally
            {
                // were we listening? 
                if (connected)
                {
                    Debug.Log(string.Format("{0}disconnected. Packets received = {1}; Packets dropped = {2}; Rendered at {3} fps", LogName, packetsReceived, packetsDropped, (packetsReceived - packetsDropped) / (DateTime.Now - connectionStarted).TotalSeconds));
                    if (!killThreadRequested)
                        Debug.Log(LogName + "Reconnecting in 1 sec");

                    statisticsReporter.RecordStreamDisconnect();
                }
            }

        }

    }


    /// <summary>
    /// Disconnects from the server, kills the thread and waits for it
    /// </summary>
    public void CloseConnection()
    {
        // prevents thread from taking up on another task
        killThreadRequested = true;

        // force closes socket
        if (socketConnection != null)
            socketConnection.Close();

        // waits for thread to end
        if (clientReceiveThread != null && clientReceiveThread.IsAlive)
            clientReceiveThread.Abort();

        // clears pointers
        clientReceiveThread = null;
        socketConnection = null;
    }

}