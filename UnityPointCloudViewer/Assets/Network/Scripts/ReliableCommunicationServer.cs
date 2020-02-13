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
/// TCP Server with support for multiple clients
/// v1.0
/// 
/// - only supports json messages for now
/// 
/// Authors: Danilo Gaques, Tommy Sharkey
/// </summary>
/// 
public class ReliableCommunicationServer : MonoBehaviour
{
    static readonly string VERSION = "1.00";

    #region public members

    [Header("Server configuration")]
    [Tooltip("This string shows up on all the logs related to this server, including its clients")]
    public string ServerName;    
    public int ListenPort;
    // Start up
    [Tooltip("Check this box if the socket should connect when the script / game object is enabled / first starts")]
    public bool ListenOnEnable = true;

    [Header("Server events")]
    bool _onStartedListening = false, _onStoppedListening = false;
    public ReliableCommunicationServerListening OnStartListening;
    public ReliableCommunicationServerStopped OnStopListening;


    // events for when a client connects, receives a message and disconnects
    [Header("Client events")]
    public ReliableCommunicationServerClientConnected OnClientConnect;
    public CommunicationJsonEvent OnJsonMessageReceived;
    public ReliableCommunicationServerClientDisconnected OnClientDisconnect;

    // internal implementation of onConnectRaised
    private Queue<ReliableCommunicationClient> justConnectedQ =  new Queue<ReliableCommunicationClient>();
    System.Object justConnectedQLock = new System.Object();

    // all clients currently connected to the server
    private HashSet<ReliableCommunicationClient> clients = new HashSet<ReliableCommunicationClient>();
    System.Object clientSetLock = new System.Object();

    // Timeouts
    [HideInInspector]
    public int ListenErrorTimeoutMs = 5000;
    [HideInInspector]
    public int WaitToAbortMs = 100;
    #endregion

    #region private members
    // TCP
    private TcpListener selfServer;
    private bool killThreadRequested = false;

    private Thread listenerThread;

    // name used for the purpose of logging
    private string LogName;
    #endregion


    /// <summary>
    /// Returns true if the server is listening for new connections
    /// </summary>
    bool _isListening = false;
    public bool isListening
    {
        get
        {
            return _isListening;
        }
    }

    // Socket statistics // TODO
    CommunicationStatistics statisticsReporter;


    #region UnityEvents
    /// <summary>
    /// Called whenever the behavior is initialized by the application
    /// </summary>
    private void Awake()
    {

        LogName = string.Format("[{0} TCP Server] - ", ServerName);

        // Statistics are currently not in use
        // making sure sockets report statistics regardless of how they were instantiated
        //statisticsReporter = GetComponent<CommunicationStatistics>();
        //if (statisticsReporter == null)
        //{
            //Debug.LogWarning(LogName + " Missing socket statistics companion");
            //statisticsReporter = this.gameObject.AddComponent<CommunicationStatistics>();
            //statisticsReporter.Name = Host.Name;
            //statisticsReporter.TCP = true;
        //}
    }

    // Update is called once per frame
    void Update()
    {

        // raises server event (onStartedListening) first
        if (_onStartedListening)
        {
            _onStartedListening = false;
            if (OnStartListening != null)
                OnStartListening.Invoke(this);
        }

        // onConnectEvents should always come before any messages
        if (justConnectedQ.Count > 0)
        {
            if (OnClientConnect != null)
                OnClientConnect.Invoke(this);

            onConnectRaised = false;
        }

        // passess all the messages that are missing
        if (messageQueue.Count > 0)
        {
            // we should not spend time processing while the queue is locked
            // as this might disconnect the socket due to timeout
            Queue<byte[]> tmpQ;
            lock (messageQueueLock)
            {
                // copies the queue from the thread
                tmpQ = messageQueue;
                messageQueue = new Queue<byte[]>();
            }

            // now we can process our messages
            while (tmpQ.Count > 0)
            {
                // process message received
                byte[] msgBytes;
                msgBytes = tmpQ.Dequeue();

                // should we drop packets?
                while (dropAccumulatedMessages && tmpQ.Count > 0)
                {
                    msgBytes = tmpQ.Dequeue();
                    statisticsReporter.RecordDroppedMessage();
                }

                // call event handlers
                if (this.EventType == CommunicationMessageType.Byte) // Byte Message
                {
                    ByteMessageReceived.Invoke(msgBytes);
                }
                else
                {
                    string msgString = Encoding.UTF8.GetString(msgBytes);

                    if (this.EventType == CommunicationMessageType.String) // String Message
                    {
                        StringMessageReceived.Invoke(msgString);
                    }
                    else // Json Message
                    {
                        JSONObject msgJson = new JSONObject(msgString);
                        OnJsonMessageReceived.Invoke(msgJson);
                    }
                }
            }
        }

        // onDisconnectEvents should be passed after all messages are sent to clients
        if (onDisconnectRaised)
        {
            _isListening = false;

            if (OnClientDisconnect != null)
                OnClientDisconnect.Invoke(this);

            onDisconnectRaised = false;
        }

        // after we are done with all the messages that were queued up so far, we stop listening if requested
        if (_onStoppedListening)
        {
            _onStoppedListening = false;
            if (OnStopListening != null)
                OnStopListening.Invoke(this);
        }
    }

    private void OnEnable()
    {
        if (ListenOnEnable)
            StartConnection();
    }

    private void OnDisable()
    {
        CloseConnection();
    }

    #endregion
    /// <summary>
    /// Call this method when you want to start a connection (either listening as a server
    /// or connecting as a client).
    /// 
    ///  if `ConnectOnEnable` is checked / True, StartConnection will be called automatically for you ;)
    /// 
    /// </summary>
    public void StartConnection()
    {
        if (listenerThread != null && listenerThread.IsAlive)
        {
            Debug.LogWarning(LogName + "Already running. Call Disconnect() first or  ForceReconnect() instead");
            return;
        }
        killThreadRequested = false;

        try
        {
            listenerThread = new Thread(new ThreadStart(ServerLoopThread));
            listenerThread.IsBackground = true;
            listenerThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError(LogName + " Failed to start socket thread: " + e);
        }
    }

    /// <summary>
    /// Closes the client connection (or the server)
    /// </summary>
    public void CloseConnection()
    {

        // Asks thread to stop listening
        killThreadRequested = true;

        // close all sockets
        try
        {
            if (selfServer != null)
            {
                selfServer.Stop();
                selfServer = null;
            }
        }
        catch (NullReferenceException)
        {
            // do nothing
        }


        // Stop Thread if it is still running
        if (listenerThread != null && listenerThread.IsAlive)
        {

            try
            {
                listenerThread.Join(WaitToAbortMs);
                if (listenerThread.IsAlive)
                    listenerThread.Abort();

                listenerThread = null;
            }
            catch (Exception)
            {
                // don't care
            }

        }

        // Is it connected? then update all members
        if (_isListening)
        {
            _isListening = false;
            _onStoppedListening = false; // we are not sure that there will be another update loop

            // disconnect all clients and invoke their final events
            // TODO

            // make sure others are aware that this socket disconnected
            if (OnStopListening != null)
                OnStopListening.Invoke(this);
        }

    }


    /// <summary>
    /// Restarts server / reconnects client
    /// </summary>
    public void ForceReconnect()
    {
        CloseConnection();
        StartConnection();
    }



    #region ReliableCommunication client/server implementation
    

    private void ServerLoopThread()
    {
        bool firstTime = true;
        _isListening = false;
        while (!killThreadRequested)
        {
            try
            {
                _isListening = false;
                if (!firstTime)
                {
                    Thread.Sleep(ListenErrorTimeoutMs);
                }
                firstTime = false;

                selfServer = new TcpListener(IPAddress.Parse("0.0.0.0"), ListenPort);
                selfServer.Start();
                Debug.Log(string.Format("{0} Listening at 0.0.0.0:{1}", LogName, ListenPort));
                _isListening = true;
                _onStartedListening = true;

                // Get a stream object for reading
                // Todo: Handle multiple clients
                while (!killThreadRequested && selfServer != null)
                {
                    try
                    {
                        TcpClient tcpClient;
                        using (tcpClient = selfServer.AcceptTcpClient())
                        {
                            //Debug.Log(string.Format("{0}Client connected {1}:{2}", LogName, ((IPEndPoint)tcpClient.Client.RemoteEndPoint).Address.ToString(), ((IPEndPoint)tcpClient.Client.RemoteEndPoint).Port));
                            //statisticsReporter.RecordConnectionEstablished();
                            
                            // creates a new client connection
                        }
                    }
                    catch (SocketException socketException)
                    {
                        if (socketException.SocketErrorCode != SocketError.Interrupted)
                        {
                            // if we didn't interrupt it -> reconnect, report statistics, log warning
                            Debug.LogError(LogName + "client socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                            statisticsReporter.RecordStreamError();
                        }
                        else
                            return; // ends thread (finally will take care of statistics and logging below)
                    }
                }
            }
            catch (SocketException socketException)
            {
                if (socketException.SocketErrorCode != SocketError.Interrupted && socketException.SocketErrorCode != SocketError.Shutdown)
                {
                    // if we didn't interrupt it (or shut it down) -> log warning and report statistics
                    Debug.LogError(LogName + "Server socket Exception: " + socketException.SocketErrorCode + "->" + socketException);
                    statisticsReporter.RecordStreamError();
                    break;
                }
                else
                {
                    return; // ends thread
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
            catch (Exception e)
            {
                // this is likely not a socket error. So while we do not record a stream error,
                // we still log for later learning about it
                Debug.LogWarning(LogName + "Exception " + e);
            }
            finally
            {
                // were we listening? 
                if (_isListening)
                {
                    if (killThreadRequested)
                        Debug.Log(LogName + "Stopped");
                    else
                        Debug.Log(LogName + "Stopped - Trying again in " + (ListenErrorTimeoutMs / 1000f) + " sec");
                }
            }
        }
    }

    #endregion

}


[System.Serializable]
public class ReliableCommunicationServerListening : UnityEvent<ReliableCommunicationServer> { }


[System.Serializable]
public class ReliableCommunicationServerStopped : UnityEvent<ReliableCommunicationServer> { }

[System.Serializable]
public class ReliableCommunicationServerClientConnected : UnityEvent<ReliableCommunicationServer, ReliableCommunicationClient> { }

[System.Serializable]
public class ReliableCommunicationServerClientDisconnected : UnityEvent<ReliableCommunication, ReliableCommunicationClient> { }
*/