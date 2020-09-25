using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class RulerBehaviour : MonoBehaviour
{

    public Transform PointA;
    public Transform PointB;
    LineRenderer line;

    // Start is called before the first frame update
    void Awake()
    {
        line = GetComponent<LineRenderer>();
    }

    // Update is called once per frame
    void Update()
    {
        line.SetPosition(0, PointA.position);
        line.SetPosition(1, PointB.position);
    }
}
