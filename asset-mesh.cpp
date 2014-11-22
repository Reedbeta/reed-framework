#include "framework.h"
#include "miniz.h"

namespace Framework
{
	// Infrastructure for compiling Wavefront .obj files to vertex/index buffers.
	//  * Currently uses hard-coded Vertex structure.
	//  * Creates a single vertex buffer and index buffer, plus a material map that
	//      identifies which faces get drawn with each material.
	//  * Deduplicates verts.
	//  * Generates normals if necessary.
	//  * Groups together all faces with the same material into a contiguous
	//      range of indices so they can be drawn with one draw call.
	//  * !!!UNDONE: Vertex cache optimization.
	//  * !!!UNDONE: Parsing the accompanying .mtl file.

	namespace OBJMeshCompiler
	{
		struct MtlRange
		{
			std::string		m_mtlName;
			int				m_iIdxStart, m_iIdxEnd;
		};

		struct Context
		{
			std::vector<Vertex>		m_verts;
			std::vector<int>		m_indices;
			std::vector<MtlRange>	m_mtlRanges;
			box3					m_bounds;
			bool					m_hasNormals;
		};

		// Prototype various helper functions
		static bool ParseOBJ(const char * path, Context * pCtxOut);
		static void DeduplicateVerts(Context * pCtx);
		static void CalculateNormals(Context * pCtx);
#if VERTEX_TANGENT
		static void CalculateTangents(Context * pCtx);
#endif
	}

	// Compiler entry point

	bool CompileOBJMeshAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut)
	{
		ASSERT_ERR(pACI);
		ASSERT_ERR(pACI->m_pathSrc);
		ASSERT_ERR(pACI->m_ack == ACK_OBJMesh);
		ASSERT_ERR(pZipOut);

		using namespace OBJMeshCompiler;

		Context ctx = {};
		if (!ParseOBJ(pACI->m_pathSrc, &ctx))
			return false;

		DeduplicateVerts(&ctx);
		if (!ctx.m_hasNormals)
			CalculateNormals(&ctx);
#if VERTEX_TANGENT
		CalculateTangents(&ctx);
#endif

		// !!!TEMP
		if (!mz_zip_writer_add_mem(pZipOut, pACI->m_pathSrc, nullptr, 0, MZ_DEFAULT_LEVEL))
		{
			ERR("Couldn't add file %s to archive", pACI->m_pathSrc);
			return false;
		}

		// !!!UNDONE
		return false;
	}



	namespace OBJMeshCompiler
	{
		static bool ParseOBJ(const char * path, Context * pCtxOut)
		{
			ASSERT_ERR(path);
			ASSERT_ERR(pCtxOut);

			// !!!UNDONE

			return false;
		}

		static void DeduplicateVerts(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}

		static void CalculateNormals(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}

#if VERTEX_TANGENT
		static void CalculateTangents(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}
#endif
	}
}
