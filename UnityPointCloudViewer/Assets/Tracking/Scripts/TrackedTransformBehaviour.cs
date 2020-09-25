using System.Collections;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// Use TrackedTransformBehaviour on a GameObject that should receive
/// updates from a tracker (e.g.: OptiTrack over the network).
/// 
/// This scripts requires a reference to TrackingHandler
/// </summary>
public class TrackedTransformBehaviour : MonoBehaviour
{

    [Header("Basic settings")]
    [Tooltip("Responsible for receiving new pose updates")]
    public TrackingHandler trackingHandler;

    [Tooltip("TrackedTransform's ID")]
    public uint id;

    [Header("Advanced Options")]
    [Tooltip("When checked, position updates will be applied as if this object is a child of another object.\nDisable and Enable script for changes to make effect")]
    public bool MakeItRelativeToAnotherTracker = false;
    private bool wasMakeItRelativeToAnotherTrackerChecked = false;

    [Tooltip("Tracker's parent ID")]
    public uint parentId;


    private void OnEnable()
    {
        // if none were set, we find the default one
        if (trackingHandler == null)
        {
            trackingHandler = TrackingHandler.FindDefault();
            if (trackingHandler != null)
                Debug.LogWarning(string.Format("[TrackedTransform's {0}] - No TrackingHandler set. Using first one available at {1}!", id, trackingHandler.gameObject.name));
        }

        if (trackingHandler == null)
        {
            Debug.LogError(string.Format("[TrackedTransform's {0}] - No TrackingHandler available!", id));
        } else
        {
            if (MakeItRelativeToAnotherTracker)
            {
                trackingHandler.SubscribeToTrackedObject(id, OnTrackedObjectUpdateRef);
            } else
            {
                trackingHandler.SubscribeToTrackedObject(id, OnTrackedObjectUpdate);
            }

            // keeps track of the older value so that OnDisable can Unsubscribe using the correct callback
            wasMakeItRelativeToAnotherTrackerChecked = MakeItRelativeToAnotherTracker;
        }

    }

    private void OnDisable()
    {

        if (wasMakeItRelativeToAnotherTrackerChecked)
        {
            trackingHandler.UnsubscribeToTrackedObject(id, OnTrackedObjectUpdateRef);
        }
        else
        {
            trackingHandler.UnsubscribeToTrackedObject(id, OnTrackedObjectUpdate);
        }
    }

    /// <summary>
    /// This method is called when a new update is received for a specific
    /// tracking object
    /// </summary>
    /// <param name="rot"></param>
    /// <param name="pos"></param>
    private void OnTrackedObjectUpdate(TrackingHandler.TrackedObject trackedObject)
    {
        this.transform.localRotation = trackedObject.rotation;
        this.transform.localPosition = trackedObject.position;
    }

    /// <summary>
    /// Similar to OnTrackedObjectUpdate, but now the position and rotation
    /// applied to the object are with respect to a second object
    /// </summary>
    /// <param name="rot"></param>
    /// <param name="pos"></param>
    private void OnTrackedObjectUpdateRef(TrackingHandler.TrackedObject trackedObject)
    {
        TrackingHandler.TrackedObject parent = this.trackingHandler.GetLatestTrackedObject(parentId);
        parent.UpdateTransformationMatrices();          // make sure we have the latest transformation matrix for this object
        trackedObject.UpdateTransformationMatrices();   // same here

        Matrix4x4 localMatrix = parent.localToGlobal * trackedObject.globalToLocal;

        this.transform.localRotation = localMatrix.rotation;
        this.transform.localPosition = localMatrix.MultiplyPoint(Vector3.zero);
    }
}
