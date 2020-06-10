using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;

[Serializable]
public class KinectJsonFormat
{
    public KinectTable table;
}

[Serializable]
public class KinectTable
{
    public string type_id;
    public int rows;
    public int cols;
    public string dt;
    public float[] data;
}

public class ReconstructionVisualizer : MonoBehaviour
{
    public GameObject RGBTextureObject;
    public GameObject DepthTextureObject;


    Renderer colorRenderer, depthRenderer;
    MeshFilter depthMeshFilter;

    Texture2D colorTexture;
    Texture2D depthTexture;
    Texture2D projectionTexture;

    Mesh pointCloudMesh;

    [System.NonSerialized]
    private Vector3[] pointCloudVertices;

    [Tooltip("Check True if using libjpegturbo if the color frame is encoded as RGB. Otherwise, leave it false for JPEG frames")]
    public bool isColorDecoded = false;

    private void Awake()
    {
        if (RGBTextureObject != null)
        {
            colorRenderer = RGBTextureObject.GetComponent<Renderer>();
        }

        if (DepthTextureObject != null)
        {
            depthRenderer = DepthTextureObject.GetComponent<Renderer>();
            depthMeshFilter = DepthTextureObject.GetComponent<MeshFilter>();
        }

        if (!SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.R16))
        {
            Debug.LogError("Your graphics card does not support R16 - depth won't render!");
        }

        pointCloudMesh = new Mesh()
        {
            indexFormat = UnityEngine.Rendering.IndexFormat.UInt32,
        };

        string path = "Assets/Resources/kinect_fastpointcloud_1280x720.json";
        StreamReader reader = new StreamReader(path);
        KinectJsonFormat kjs = JsonUtility.FromJson<KinectJsonFormat>(reader.ReadToEnd());

        Debug.Log("Reading Kinect " + kjs.table.cols + "x" + kjs.table.rows + " Table from (" + path + ")");

        Color[] colors = new Color[kjs.table.cols * kjs.table.rows];

        for (int i = 0; i < colors.Length; i++)
        {
            colors[i].r = kjs.table.data[2 * i];
            colors[i].g = kjs.table.data[2 * i + 1];
        }

        projectionTexture = new Texture2D(kjs.table.cols, kjs.table.rows, TextureFormat.RGFloat, false)
        {
            wrapMode = TextureWrapMode.Clamp,
            filterMode = FilterMode.Point,
        };
        projectionTexture.SetPixels(colors);
        projectionTexture.Apply();

        //var materialProperty = new MaterialPropertyBlock();

        //materialProperty.SetFloatArray("_PointCastTable", v2arr);
        //depthRenderer.SetPropertyBlock(materialProperty);
    }



    void ResetMesh(int width, int height)
    {
        Debug.Log(string.Format("[ReconstructionVisualizer] - Updating point cloud mesh to {0}x{1} pixels", width, height));
        pointCloudMesh.Clear();

        pointCloudVertices = new Vector3[width * height];

        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                pointCloudVertices[i + j * width] = new Vector3((i - (width / 2.0f)) * 0.01f,
                                                                (j - (height / 2.0f)) * 0.01f,
                                                                0.0f);
            }
        }

        var indices = new int[pointCloudVertices.Length];
        for (int i = 0; i < pointCloudVertices.Length; i++)
            indices[i] = i;

        // make sure vertices are dynamic?
        pointCloudMesh.MarkDynamic();
        pointCloudMesh.vertices = pointCloudVertices;

        // uv map for texture
        var uvs = new Vector2[width * height];
        Array.Clear(uvs, 0, uvs.Length);
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                uvs[i + j * width].x = i / (float)width;
                uvs[i + j * width].y = j / (float)height;
            }
        }

        pointCloudMesh.uv = uvs;
        pointCloudMesh.SetIndices(indices, MeshTopology.Points, 0, false);
        pointCloudMesh.bounds = new Bounds(Vector3.zero, Vector3.one * 10f);
        depthMeshFilter.sharedMesh = pointCloudMesh;

        //pointCloudMesh
    }


    public void onMeshReady(int width, int height, byte[] depthBuffer, byte[] colorBuffer)
    {
        //Debug.Log(string.Format("Received a picture of size {0}x{1}: {2} bytes", width, height, colorBuffer.Length));
        // allocates depth texture if not previsouly allocated
        if (depthTexture == null)
        {
            ResetMesh(width, height);
            depthTexture = new Texture2D(width, height, TextureFormat.R16, false)
            {
                wrapMode = TextureWrapMode.Clamp,
                filterMode = FilterMode.Point,
            };
            depthRenderer.material.SetTexture("_DepthTex", depthTexture);
            // associate texture with shader
            depthRenderer.material.SetTexture("_ProjectionTex", projectionTexture);

            colorTexture = new Texture2D(width, height, TextureFormat.RGB24, false);


            colorRenderer.material.mainTexture = colorTexture;
            depthRenderer.material.SetTexture("_ColorTex", colorTexture);
        }
        else if (depthTexture.width != width || depthTexture.height != height)
        {

            ResetMesh(width, height);
            depthTexture = new Texture2D(width, height, TextureFormat.R16, false)
            {
                wrapMode = TextureWrapMode.Clamp,
                filterMode = FilterMode.Point,
            };
            depthRenderer.material.SetTexture("_DepthTex", depthTexture);
            // associate texture with shader

            // creates a new texture
            colorTexture = new Texture2D(width, height, TextureFormat.RGB24, false);

            colorRenderer.material.mainTexture = colorTexture;
            depthRenderer.material.SetTexture("_ColorTex", colorTexture);
        }

        // load color texture
        if (isColorDecoded)
        {
            colorTexture.LoadRawTextureData(colorBuffer);
            colorTexture.Apply();
        }
        else
            colorTexture.LoadImage(colorBuffer, false);


        // load depth texture
        depthTexture.LoadRawTextureData(depthBuffer);
        depthTexture.Apply();
        // depthTextur
    }


}
