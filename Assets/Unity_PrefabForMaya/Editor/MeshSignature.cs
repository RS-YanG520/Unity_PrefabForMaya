using System.Globalization;
using System.Text;
using UnityEngine;

namespace MayaPrefabBuilder
{
    /// <summary>
    /// 计算几何体签名，用于识别 FBX 中由 Maya 实例展开的相同网格。
    /// </summary>
    public static class MeshSignature
    {
        private const ulong FnvOffsetBasis = 14695981039346656037UL;
        private const ulong FnvPrime = 1099511628211UL;

        /// <summary>
        /// 计算某个实例候选对象（含整个子树）的几何签名。
        /// 按层级顺序 DFS 遍历，对每个非空 Mesh 依次折叠哈希。
        /// </summary>
        public static string ComputeSignature(GameObject root)
        {
            var sb = new StringBuilder();
            CollectRecursive(root.transform, sb);
            return sb.ToString();
        }

        private static void CollectRecursive(Transform t, StringBuilder sb)
        {
            var filter = t.GetComponent<MeshFilter>();
            if (filter != null && filter.sharedMesh != null)
            {
                sb.Append(ComputeMeshSignature(filter.sharedMesh)).Append('|');
            }

            for (int i = 0; i < t.childCount; i++)
            {
                CollectRecursive(t.GetChild(i), sb);
            }
        }

        /// <summary>
        /// 计算单个 Mesh 的签名。
        /// 始终包含 vertexCount / subMeshCount / bounds / 各 submesh 索引数（这些无需 isReadable 即可读取）；
        /// 当 mesh 可读（isReadable）时，额外折叠顶点、法线、UV、三角形的原始字节以消除假阳性。
        /// </summary>
        public static string ComputeMeshSignature(Mesh mesh)
        {
            var sb = new StringBuilder();
            sb.Append("v").Append(mesh.vertexCount);
            sb.Append("s").Append(mesh.subMeshCount);
            sb.Append("b").Append(Round(mesh.bounds.center)).Append(Round(mesh.bounds.size));

            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                sb.Append('i').Append(mesh.GetIndexCount(i));
            }

            if (mesh.isReadable)
            {
                sb.Append('h').Append(HashVector3(mesh.vertices));
                if (mesh.normals != null && mesh.normals.Length > 0)
                    sb.Append('n').Append(HashVector3(mesh.normals));
                if (mesh.uv != null && mesh.uv.Length > 0)
                    sb.Append('u').Append(HashVector2(mesh.uv));
                for (int i = 0; i < mesh.subMeshCount; i++)
                {
                    sb.Append('t').Append(HashInts(mesh.GetTriangles(i)));
                }
            }

            return sb.ToString();
        }

        private static string Round(Vector3 v)
        {
            var inv = CultureInfo.InvariantCulture;
            return v.x.ToString("F4", inv) + "," + v.y.ToString("F4", inv) + "," + v.z.ToString("F4", inv);
        }

        private static ulong HashVector3(Vector3[] data)
        {
            unchecked
            {
                ulong hash = FnvOffsetBasis;
                foreach (var v in data)
                {
                    hash = FnvFloat(hash, v.x);
                    hash = FnvFloat(hash, v.y);
                    hash = FnvFloat(hash, v.z);
                }
                return hash;
            }
        }

        private static ulong HashVector2(Vector2[] data)
        {
            unchecked
            {
                ulong hash = FnvOffsetBasis;
                foreach (var v in data)
                {
                    hash = FnvFloat(hash, v.x);
                    hash = FnvFloat(hash, v.y);
                }
                return hash;
            }
        }

        private static ulong HashInts(int[] data)
        {
            var bytes = new byte[data.Length * sizeof(int)];
            System.Buffer.BlockCopy(data, 0, bytes, 0, bytes.Length);
            return Fnv1aBytes(bytes);
        }

        private static ulong FnvFloat(ulong hash, float value)
        {
            // 用 float 的位模式（bit pattern），保证 -0 与 0、NaN 也按字节区分
            int bits = System.BitConverter.ToInt32(System.BitConverter.GetBytes(value), 0);
            return FnvUint(hash, unchecked((uint)bits));
        }

        private static ulong FnvUint(ulong hash, uint value)
        {
            unchecked
            {
                hash ^= value & 0xFF;
                hash *= FnvPrime;
                hash ^= (value >> 8) & 0xFF;
                hash *= FnvPrime;
                hash ^= (value >> 16) & 0xFF;
                hash *= FnvPrime;
                hash ^= (value >> 24) & 0xFF;
                hash *= FnvPrime;
                return hash;
            }
        }

        private static ulong Fnv1aBytes(byte[] bytes)
        {
            unchecked
            {
                ulong hash = FnvOffsetBasis;
                foreach (var b in bytes)
                {
                    hash ^= b;
                    hash *= FnvPrime;
                }
                return hash;
            }
        }
    }
}
