using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.Events;

/// <summary>
/// TrackingHandler receives tracking info (id, rotation, and position) from external sources
/// (e.g.: network) and informs subscribers of the updates.
/// 
/// TrackingHandler also keeps track of the last tracking info received per object so that they
/// can be queried after fact
/// 
/// </summary>
public class TrackingHandler : MonoBehaviour
{
    /// <summary>
    /// TrackedObject is a tuple that holds both a rotation and a position.
    /// </summary>
    public class TrackedObject
    {
        public Quaternion rotation;
        public Vector3 position;
        public double networkTimestamp;
        public DateTime deviceTimestamp;

        public bool matricesCalculated;
        public Matrix4x4 localToGlobal;
        public Matrix4x4 globalToLocal;

        public int updateCounter;


        DateTime firstMessageDT = DateTime.Now;

        public TrackedObject()
        {
            firstMessageDT = DateTime.Now;
            rotation = Quaternion.identity;
            position = Vector3.zero;
            UpdateTransformationMatrices();
        }

        /// <summary>
        /// Returns the total number of updates per second
        /// (Assuming last message was received now)
        /// </summary>
        /// <returns></returns>
        public double TotalUpdatesPerSecond()
        {
            return updateCounter / (DateTime.Now - firstMessageDT).TotalSeconds;
        }

        public void UpdateRotationLocation(double timestamp, Quaternion r, Vector3 t)
        {
            // updates timestamps
            networkTimestamp = timestamp;
            deviceTimestamp = DateTime.Now;

            rotation = r;
            position = t;

            updateCounter++;

            matricesCalculated = false;
        }

        public void UpdateTransformationMatrices()
        {
            if (!matricesCalculated)
            {
                matricesCalculated = true;
                globalToLocal = Matrix4x4.TRS(position, rotation, Vector3.one);
                localToGlobal = globalToLocal.inverse;
                
            }
        }
    }

    private Dictionary<UInt32, TrackedObject> trackedObjectsState = new Dictionary<uint, TrackedObject>();
    private Dictionary<UInt32, TrackedObjectUpdatedEventHandler> trackedObjectsSubscribers = new Dictionary<uint, TrackedObjectUpdatedEventHandler>();
    

    /// <summary>
    /// Returns a reference to id's "id" latest rotation and position.
    /// 
    /// This method never returns null. Thus, requesting an id that never
    /// received an update will return the identity quaternion and the zero vector
    /// </summary>
    /// <param name="id">The id of the tracked object</param>
    /// <returns>TrackedObject</returns>
    public TrackedObject GetLatestTrackedObject(UInt32 id)
    {
        return GetOrCreateTrackedObject(id);
    }

    /// <summary>
    /// Returns the first <see cref="TrackingHandler"/> component located in the scene.
    /// </summary>
    /// <returns>The first TrackingHandler in the scene or null if none are found.</returns>
    public static TrackingHandler FindDefault()
    {
        TrackingHandler[] allTrackingHandlers = FindObjectsOfType<TrackingHandler>();

        if (allTrackingHandlers.Length == 0)
        {
            Debug.LogError("[TrackingHandler] - No TrackingHandlers set in your scene!");
            return null;
        }
        else if (allTrackingHandlers.Length > 1)
        {
            Debug.LogWarning("[TrackingHandler] - Multiple TrackingHandlers in your scene. Make sure that all your TrackedTransformBehaviours are referrencing to the 'correct' one!");
        }

        return allTrackingHandlers[0];
    }


    #region UpdateTrackedObject implementation
   

    public void UpdateTrackedObject(double timestamp, UInt32 id, Vector3 position, Quaternion rotation)
    {
        TrackedObject t = GetOrCreateTrackedObject(id);
        t.UpdateRotationLocation(timestamp, rotation, position);

        UpdateSubscribers(id, t);
    }

    #endregion

    #region Methods to subscribe/unsubscribe
    public void SubscribeToTrackedObject(UInt32 id, UnityAction<TrackedObject> ttb)
    {
        // get or create an event handler
        TrackedObjectUpdatedEventHandler eventHandlers;
        if (!trackedObjectsSubscribers.TryGetValue(id, out eventHandlers))
        {
            eventHandlers = new TrackedObjectUpdatedEventHandler();
            trackedObjectsSubscribers[id] = eventHandlers;
        }

        // subscribes to the event
        eventHandlers.AddListener(ttb);
        Debug.Log("[TrackingHandler] - Listening for id " + id);
    }

    public void UnsubscribeToTrackedObject(UInt32 id, UnityAction<TrackedObject> tbb)
    {
        // get the event handler responsible for id
        TrackedObjectUpdatedEventHandler eventHandlers;
        if (trackedObjectsSubscribers.TryGetValue(id, out eventHandlers))
        {
            eventHandlers.RemoveListener(tbb);
        }

        // do nothing if not event handlers were created for id "id"
    }

    #endregion

    #region Private Aux Methods
    private TrackedObject GetOrCreateTrackedObject(UInt32 id)
    {
        TrackedObject t;

        // gets tracked object if it exists, creates it if not
        if (!trackedObjectsState.TryGetValue(id, out  t))
        {
            // creates tracked object
            t = new TrackedObject();
            trackedObjectsState[id] = t;
        }

        return t;
    }

    /// <summary>
    /// This internal method invokes all callbacks associated with an id, 
    /// passing them the value of the most recent update
    /// </summary>
    /// <param name="id">TrackedObject's id</param>
    /// <param name="it">TrackedObject rotation and translation</param>
    private void UpdateSubscribers(UInt32 id, TrackedObject t)
    {
        TrackedObjectUpdatedEventHandler handlers;
        if (trackedObjectsSubscribers.TryGetValue(id, out handlers))
        {
            handlers?.Invoke(t);
        }
    }
    #endregion

    #region Unity event handlers

  

    private void OnDisable()
    {
        StringBuilder trackingHandlerLog = new StringBuilder();

        trackingHandlerLog.AppendLine("[TrackingHandler] - Summary of all updates received");

        foreach (KeyValuePair<UInt32, TrackedObject> e in trackedObjectsState)
        {
            trackingHandlerLog.Append("\t");
            trackingHandlerLog.Append(e.Key);
            trackingHandlerLog.Append(" -> ");
            trackingHandlerLog.Append(e.Value.TotalUpdatesPerSecond());
            trackingHandlerLog.Append(" updates per second \n");
        }

        Debug.Log(trackingHandlerLog.ToString());
    }

    #endregion

    [System.Serializable]
    public class TrackedObjectUpdatedEventHandler : UnityEvent<TrackedObject> { }
}
