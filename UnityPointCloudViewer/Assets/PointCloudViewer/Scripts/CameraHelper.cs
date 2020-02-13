using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CameraHelper : MonoBehaviour
{
    [Tooltip("width")]
    public uint lengthX = 640;

    [Tooltip("height")]
    public uint lengthY = 576;

    public float focalLengthX = 504.458f;
    public float focalLengthY = 504.438f;

    [Range(1, 179)]
    public float fovY = 75.0f;

    [Range(1, 179)]
    public float fovX = 65.0f;

    [Tooltip("clipping plane min in meters (for visualization purposes)")]
    public float clippingPlaneMin = 0.5f;

    [Tooltip("clipping plane max in meters (for visualization purposes)")]
    public float clippingPlaneMax = 3.86f;

    public float principalPointX = 328.616f;
    public float principalPointY = 344.08f;


    [Tooltip("When true, it applies the parameters above to the visualization")]
    public bool UpdateView;

    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
