using System.Collections;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// MotivePose describes the pose (translation and rotation)
/// of a motive rigid body at a certain point in time
/// </summary>
public class MotivePose
{
    public double timestamp;
    public uint id;
    public Vector3 translation;
    public Quaternion rotation;

    public MotivePose(uint nuid, double ntimestamp) { id = nuid; timestamp = ntimestamp; }
}
