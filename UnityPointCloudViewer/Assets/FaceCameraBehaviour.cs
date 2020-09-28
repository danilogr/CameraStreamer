using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class FaceCameraBehaviour : MonoBehaviour
{

    LineRenderer line;
    TextMesh text;

    private void Awake()
    {
        line = GetComponentInParent<LineRenderer>();
        text = GetComponent<TextMesh>();
    }

    // Update is called once per frame
    void Update()
    {
        Vector3 midPoint = (line.GetPosition(0) + line.GetPosition(1)) / 2.0f;
        transform.position = midPoint;
        transform.LookAt(midPoint, Vector3.up);

        // update text
        text.text = String.Format("{0,5:0.0000} m", Vector3.Distance(line.GetPosition(1), line.GetPosition(0)));



    }
}
