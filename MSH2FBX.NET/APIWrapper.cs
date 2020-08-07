using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace MSH2FBX
{
    internal static class APIWrapper
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void LogCallback([MarshalAs(UnmanagedType.LPStr)] string msg, byte type);

        // CONSTRUCTORS - DESTRUCTOR //
        [DllImport("MSH2FBX")]
        public static extern IntPtr Converter_Create();

        [DllImport("MSH2FBX")]
        public static extern IntPtr Converter_Create_Start([MarshalAs(UnmanagedType.LPStr)] string fbxFileName);

        [DllImport("MSH2FBX")]
        public static extern void Converter_Destroy(IntPtr converter);

        // PROPERTIES //
        [DllImport("MSH2FBX")]
        public static extern void Converter_Set_ModelIgnoreFilter(IntPtr converter, ushort filter);

        [DllImport("MSH2FBX")]
        public static extern ushort Converter_Get_ModelIgnoreFilter(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern void Converter_Set_ChunkFilter(IntPtr converter, byte filter);

        [DllImport("MSH2FBX")]
        public static extern byte Converter_Get_ChunkFilter(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern void Converter_Set_OverrideAnimName(IntPtr converter, [MarshalAs(UnmanagedType.LPStr)] string animName);

        [DllImport("MSH2FBX")]
        [return:MarshalAs(UnmanagedType.LPStr)]
        public static extern string Converter_Get_OverrideAnimName(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern void Converter_Set_EmptyMeshes(IntPtr converter, bool emptyMeshes);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_Get_EmptyMeshes(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern EChunkFilter Converter_Set_BaseposeMSH(IntPtr converter, [MarshalAs(UnmanagedType.LPStr)] string baseposeMSH);

        [DllImport("MSH2FBX")]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static extern string Converter_Get_BaseposeMSH(IntPtr converter);

        // METHODS //
        [DllImport("MSH2FBX")]
        public static extern void Converter_SetLogCallback(LogCallback callback);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_Start(IntPtr converter, [MarshalAs(UnmanagedType.LPStr)] string fbxFileName);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_AddMSHFromPath(IntPtr converter, [MarshalAs(UnmanagedType.LPStr)] string mshFileName);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_AddMSHFromPtr(IntPtr converter, IntPtr msh);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_SaveFBX(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern bool Converter_ClearFBXScene(IntPtr converter);

        [DllImport("MSH2FBX")]
        public static extern void Converter_Close(IntPtr converter);
    }
}
