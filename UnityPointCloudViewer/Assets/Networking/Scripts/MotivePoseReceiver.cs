using System.Collections;
using System.Collections.Generic;
using System;

using System.Net.Sockets;
using System.Threading;
using System.Net;


using UnityEngine;
using System.Text;
using System.Data;
using UnityEngine.Events;

/// <summary>
/// UDP socket for receiving custom Motive poses
/// Author: Danilo Gasques
/// </summary>
/// 
public class MotivePoseReceiver : MonoBehaviour
{

    


    [Tooltip("Used for logs so that we know which class is throwing errors")]
    public string Name;

    [Tooltip("Port number the socket should be using for listening")]
    public int port = 12345;

    [HideInInspector]
    public int WaitToAbortMs = 100;

    [Tooltip("Event that gets called when a motive pose is received")]
    public MotivePoseReceivedEvent OnMotivePoseReceived = new MotivePoseReceivedEvent();

    [HideInInspector]
    /// keeps track of all the poses updated between Update calls
    private Dictionary<uint, MotivePose> lastPoses = new Dictionary<uint, MotivePose>();
    double lastTimestamp = 0;

    private object lastPosesMapLock = new object();

    #region Event
    /// <summary>
    /// Called when ReliableCommunicationSocket raises an error (int -> socket error; string -> message converted to text)
    /// </summary>
    [System.Serializable]
    public class MotivePoseReceivedEvent : UnityEvent<double, uint, Vector3, Quaternion> { }
    #endregion

    private UdpClient _impl;
    private string LogName;
    IPEndPoint anyIP = new IPEndPoint(IPAddress.Any, 0);  // listens to any address
    bool stopThread = false;
    Thread socketThread;

    // our short statistics
    uint packetsReceived = 0;
    uint packetsIgnored = 0;
    uint packetsInvalid = 0;
    uint packetsOutOfOrder = 0;
    DateTime onEnabledTime;
    DateTime onDisabledTime;


    // CommunicationStatistics statisticsReporter;

    // ================================================================================ Unity Events =========================================================== //
    /// <summary>
    /// Socket constructor, creates all socket objects and defines each socket unique id
    /// </summary>

    public void Awake()
    {
        if (Name.Length == 0)
            Name = this.gameObject.name;

        LogName = "[" + Name + " UDP] - ";

        // making sure sockets report statistics regardless of how they were instantiated
        /*statisticsReporter = GetComponent<CommunicationStatistics>();
        if (statisticsReporter == null)
        {
            //Debug.LogWarning(LogName + " Missing socket statistics companion");
            statisticsReporter = this.gameObject.AddComponent<CommunicationStatistics>();
            statisticsReporter.Name = Name;
            statisticsReporter.TCP = false;
        }*/

    }

    /// <summary>
    /// Called every frame
    /// </summary>
    void Update()
    {
        // are we waiting for messages ?
        if (lastPoses.Count > 0)
        {
            // process message received
            Dictionary<uint, MotivePose> tmpPoses;
            

            lock (lastPosesMapLock)
            {
                // copies the queue from the thread
                tmpPoses = lastPoses;
                lastPoses = new Dictionary<uint, MotivePose>();
            }

            // invokes events for each rigid body updated
            foreach (var idPosePair in tmpPoses)
            {
                OnMotivePoseReceived?.Invoke(idPosePair.Value.timestamp, idPosePair.Key, idPosePair.Value.translation, idPosePair.Value.rotation);
            }

        }
    }

    // whenever socket is enabled, we enable the thread (The socket won't run if disabled)
    private void OnEnable()
    {
      

        // creates the socket
        try
        {
            _impl = new UdpClient(port);
            socketThread = new Thread(new ThreadStart(SocketThreadLoop));
            socketThread.IsBackground = true;
            stopThread = false;
            socketThread.Start();
            Debug.Log(string.Format("{0}Listening on port {1} for {2} events", LogName, port, "RigidBody"));

            // reet stistics
            packetsReceived = 0;
            packetsIgnored = 0;
            packetsInvalid = 0;
            packetsOutOfOrder = 0;
            onEnabledTime = onDisabledTime = DateTime.Now;
        }
        catch (Exception err)
        {
            Debug.LogError(string.Format("{0}Unable to start - {1}", LogName, err.ToString()));

            if (socketThread != null)
                socketThread.Abort();
            socketThread = null;
            stopThread = true;
            _impl = null;
        }


    }


    // whenever socket is disabled, we stop listening for packets
    private void OnDisable()
    {

        if (socketThread != null)
        {
            // asks thread to stopp
            stopThread = true;

            // closes socket
            _impl.Close();

            // aborts thread if it is still running
            if (socketThread.IsAlive)
            {
                socketThread.Join(WaitToAbortMs); // waits WaitToAbortMs ms
                if (socketThread.IsAlive)
                    socketThread.Abort();

            }

            // report stats
            onDisabledTime = DateTime.Now;
            double totalExecutionTime = (onDisabledTime - onEnabledTime).TotalMilliseconds;
            double packetsPerSecond = ((double)(packetsReceived - packetsIgnored - packetsOutOfOrder - packetsInvalid) / totalExecutionTime) * 1000.0;
            double totalPacketsperSecond = ((double)(packetsReceived) / totalExecutionTime) * 1000.0;
            Debug.Log(string.Format("{0}Received a total of {1} msgs/s ({2} out of order, {3} invalid, {4} ignored). Parsed {5} msgs/s in a total of {6} minutes", LogName, totalPacketsperSecond, packetsOutOfOrder, packetsInvalid, packetsIgnored, packetsPerSecond, totalExecutionTime / 60000.0));

            // reports stats
            //statisticsReporter.RecordStreamDisconnect();

            // disposes of objects
            socketThread = null;

            _impl = null;

        }

        // erases queue

        Debug.Log(string.Format("{0}Stopped", LogName));
    }


    // ==================================== Code that only runs on the Unity Editor / Windows Desktop ======================================================//

    // Windows thread loop
    /// <summary>
    /// Socket Thread Loop for the socket version running on Windows
    /// </summary>
    private void SocketThreadLoop()
    {
        
        while (!stopThread)
        {
            try
            {
                byte[] msg = _impl.Receive(ref anyIP);
                ++packetsReceived;

                // the message should have 40 bytes: uint (4 bytes), timestamp (double 8 bytes), vector3 (12 bytes), quaternion (16 bytes)
                if (msg.Length == 40)
                {
                    double timestamp = BitConverter.ToDouble(msg, 0);
                    UInt32 id = BitConverter.ToUInt32(msg, 8);
                    
                    // do we care about this pose?
                    if (timestamp >= lastTimestamp)
                    {
                        lastTimestamp = timestamp;
                        lock (lastPosesMapLock)
                        {
                            MotivePose lastPose;

                            // we already had pose
                            if (lastPoses.TryGetValue(id, out lastPose))
                            {
                                lastPose.timestamp = timestamp;
                                ++packetsIgnored; // ignoring old packet in lieu of the newer one
                            }
                            else
                            {
                                lastPoses[id] = lastPose = new MotivePose(id, timestamp);
                            }

                            // updates motive pose object
                            lastPose.translation.x = BitConverter.ToSingle(msg, 12);
                            lastPose.translation.y = BitConverter.ToSingle(msg, 16);
                            lastPose.translation.z = BitConverter.ToSingle(msg, 20);

                            lastPose.rotation.x = BitConverter.ToSingle(msg, 24);
                            lastPose.rotation.y = BitConverter.ToSingle(msg, 28);
                            lastPose.rotation.z = BitConverter.ToSingle(msg, 32);
                            lastPose.rotation.w = BitConverter.ToSingle(msg, 36);
                        }
                    }
                    else
                    {
                        ++packetsOutOfOrder;
                    }
                }
                else
                {
                    ++packetsInvalid;
                }

            }
            catch (ThreadAbortException)
            { }
            catch (System.Net.Sockets.SocketException socketException)
            {
                // if we didn't interrupt it -> reconnect, report statistics, log warning
                if (socketException.SocketErrorCode != SocketError.Interrupted)
                {
                    Debug.LogError(LogName + "Socket Error: " + socketException.SocketErrorCode + "->" + socketException);
                }

            }
            catch (Exception err)
            {
                Debug.LogError(LogName + "Generic Exception -> " + err.ToString());
            }
        }
    }
}
