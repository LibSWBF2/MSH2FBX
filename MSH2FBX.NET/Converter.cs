using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MSH2FBX
{
    public enum ELogType : byte
    {
        Info = 0,
        Warning = 1,
        Error = 2
    }

    // Bitmap flags
    public enum EModelPurpose : ushort
    {
        // Unknown
        Miscellaneous = 0,

        // Meshes
        Mesh = 63,
        Mesh_Regular = 1,
        Mesh_Lowrez = 2,
        Mesh_Collision = 4,
        Mesh_VehicleCollision = 8,
        Mesh_ShadowVolume = 16,
        Mesh_TerrainCut = 32,

        // Just Points
        Point = 448,
        Point_EmptyTransform = 64,
        Point_DummyRoot = 128,
        Point_HardPoint = 256,

        // Skeleton
        Skeleton = 7680,
        Skeleton_Root = 512,
        Skeleton_BoneRoot = 1024,
        Skeleton_BoneLimb = 2048,
        Skeleton_BoneEnd = 4096,
    };

    // Bit Flags
    // Set a flag for all types you DON't want to be handled (filtered out)
    public enum EChunkFilter : byte
    {
        None = 0,
        Materials = 1,
        Models = 2,			// includes empty nodes, bones, etc.
        Animations = 4,
        Weights = 8
    }

    public class Converter
    {
        public static Action<string, ELogType> OnLog;
        static APIWrapper.LogCallback callback = new APIWrapper.LogCallback((string msg, byte type) => 
        {
            OnLog(msg, (ELogType)type);
        });

        IntPtr Instance;

        public EModelPurpose ModelFilter
        {
            get { return (EModelPurpose)APIWrapper.Converter_Get_ModelIgnoreFilter(Instance); }
            set { APIWrapper.Converter_Set_ModelIgnoreFilter(Instance, (ushort)value); }
        }

        public EChunkFilter ChunkFilter
        {
            get { return (EChunkFilter)APIWrapper.Converter_Get_ChunkFilter(Instance); }
            set { APIWrapper.Converter_Set_ChunkFilter(Instance, (byte)value); }
        }

        public string OverrideAnimName
        {
            get { return APIWrapper.Converter_Get_OverrideAnimName(Instance); }
            set { APIWrapper.Converter_Set_OverrideAnimName(Instance, value); }
        }

        public bool EmptyMeshes
        {
            get { return APIWrapper.Converter_Get_EmptyMeshes(Instance); }
            set { APIWrapper.Converter_Set_EmptyMeshes(Instance, value); }
        }

        public string BaseposeMSH
        {
            get { return APIWrapper.Converter_Get_BaseposeMSH(Instance); }
            set { APIWrapper.Converter_Set_BaseposeMSH(Instance, value); }
        }


        static Converter()
        {
            APIWrapper.Converter_SetLogCallback(callback);
        }

        public Converter()
        {
            Instance = APIWrapper.Converter_Create();
        }

        public Converter(string fbxFileName)
        {
            Instance = APIWrapper.Converter_Create_Start(fbxFileName);
        }

        ~Converter()
        {
            APIWrapper.Converter_Destroy(Instance);
        }


        public bool Start(string fbxFileName)
        {
            return APIWrapper.Converter_Start(Instance, fbxFileName);
        }

        public bool AddMSH(string mshFileName)
        {
            return APIWrapper.Converter_AddMSHFromPath(Instance, mshFileName);
        }

        public bool AddMSH(IntPtr MshPtr)
        {
            return APIWrapper.Converter_AddMSHFromPtr(Instance, MshPtr);
        }

        public bool SaveFBX()
        {
            return APIWrapper.Converter_SaveFBX(Instance);
        }

        public bool ClearFBXScene()
        {
            return APIWrapper.Converter_ClearFBXScene(Instance);
        }

        public void Close()
        {
            APIWrapper.Converter_Close(Instance);
        }
    }
}