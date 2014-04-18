#include "camera.h"

#include <cassert>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Framework
{
	// Camera implementation

	Camera::Camera()
	:	m_mbuttonCur(MBUTTON_None),
		m_wheelDelta(0),
		m_worldToClip(float4x4::identity())
	{
	}

	

	// PerspectiveCamera implementation

	PerspectiveCamera::PerspectiveCamera()
	:	super(),
		m_viewToWorld(affine3::identity()),
		m_worldToView(affine3::identity()),
		m_projection(float4x4::identity())
	{
	}

	void PerspectiveCamera::SetProjection(
		float vFOV,
		float aspect,
		float zNear,
		float zFar)
	{
		m_projection = perspProjD3DStyle(vFOV, aspect, zNear, zFar);
		UpdateWorldToClip();
	}

	void PerspectiveCamera::UpdateWorldToClip()
	{
		m_worldToView = transpose(m_viewToWorld);
		m_worldToClip = affineToHomogeneous(m_worldToView) * m_projection;
	}



	// FPSCamera implementation

	FPSCamera::FPSCamera()
	:	super(),
		m_moveSpeed(1.0f),
		m_rotateSpeed(0.005f),
		m_mbuttonActivate(MBUTTON_None),
		m_mousePosPrev(makeipoint2(0, 0)),
		m_yaw(0.0f),
		m_pitch(0.0f),
		m_pos(makepoint3(0, 0, 0))
	{
	}

	void FPSCamera::Update(float timestep)
	{
		// Track mouse motion
		ipoint2 mousePos;
		GetCursorPos((POINT *)&mousePos);
		int2 mouseMove = mousePos - m_mousePosPrev;
		m_mousePosPrev = mousePos;

		// Handle mouse rotation
		if (m_mbuttonActivate == MBUTTON_None ||
			m_mbuttonCur == m_mbuttonActivate)
		{
			m_yaw -= m_rotateSpeed * mouseMove.x;
			m_yaw = modPositive(m_yaw, 2.0f*pi);
			
			m_pitch -= m_rotateSpeed * mouseMove.y;
			m_pitch = clamp(m_pitch, -0.5f*pi, 0.5f*pi);
		}

		UpdateOrientation();

		// Handle motion
		// !!!UNDONE: acceleration based on how long you've been holding the button,
		// to make fine motions easier?
		float moveStep = timestep * m_moveSpeed;
		// !!!UNDONE: move keyboard tracking into an input system that respects focus, etc.
		if (GetAsyncKeyState(VK_SHIFT))
			moveStep *= 3.0f;
		if (GetAsyncKeyState('W'))
			m_pos -= m_viewToWorld.m_linear[2] * moveStep;
		if (GetAsyncKeyState('S'))
			m_pos += m_viewToWorld.m_linear[2] * moveStep;
		if (GetAsyncKeyState('A'))
			m_pos -= m_viewToWorld.m_linear[0] * moveStep;
		if (GetAsyncKeyState('D'))
			m_pos += m_viewToWorld.m_linear[0] * moveStep;
		if (GetAsyncKeyState('E'))
			m_pos += m_viewToWorld.m_linear[1] * moveStep;
		if (GetAsyncKeyState('C'))
			m_pos -= m_viewToWorld.m_linear[1] * moveStep;

		// Calculate remaining matrices
		m_viewToWorld.m_translation = makefloat3(m_pos);
		UpdateWorldToClip();
	}

	void FPSCamera::LookAt(point3 posCamera, point3 posTarget)
	{
		m_pos = posCamera;
		m_viewToWorld.m_translation = makefloat3(m_pos);

		// Calculate angles to look at posTarget
		float3 vecToTarget = posTarget - posCamera;
		if (all(isnear(vecToTarget, 0.0f)))
		{
			assert(false);
			return;
		}
		vecToTarget = normalize(vecToTarget);
		m_yaw = atan2f(-vecToTarget.z, vecToTarget.x);
		m_pitch = asinf(vecToTarget.y);

		// Update matrices
		UpdateOrientation();
		UpdateWorldToClip();
	}

	void FPSCamera::UpdateOrientation()
	{
		// Calculate new orientation matrix using spherical coordinates
		float cosYaw = cosf(m_yaw);
		float sinYaw = sinf(m_yaw);
		float cosPitch = cosf(m_pitch);
		float sinPitch = sinf(m_pitch);
		float3 forward = { cosYaw * cosPitch, sinPitch, -sinYaw * cosPitch };
		float3 right = { sinYaw, 0, cosYaw };
		float3 up = cross(right, forward);
		m_viewToWorld.m_linear = makefloat3x3(right, up, -forward);
	}



	// MayaCamera implementation

	MayaCamera::MayaCamera()
	:	super(),
		m_rotateSpeed(0.005f),
		m_zoomSpeed(0.01f),
		m_zoomWheelSpeed(0.001f),
		m_mousePosPrev(makeipoint2(0, 0)),
		m_yaw(0.0f),
		m_pitch(0.0f),
		m_posTarget(makepoint3(0, 0, 0)),
		m_radius(1.0f),
		m_pos(makepoint3(-1, 0, 0))
	{
	}

	void MayaCamera::Update(float timestep)
	{
		(void)timestep;

		// Track mouse motion
		ipoint2 mousePos;
		GetCursorPos((POINT *)&mousePos);
		int2 mouseMove = mousePos - m_mousePosPrev;
		m_mousePosPrev = mousePos;

		// Handle mouse rotation
		if (m_mbuttonCur == MBUTTON_Left)
		{
			m_yaw -= m_rotateSpeed * mouseMove.x;
			m_yaw = modPositive(m_yaw, 2.0f*pi);
			
			m_pitch -= m_rotateSpeed * mouseMove.y;
			m_pitch = clamp(m_pitch, -0.5f*pi, 0.5f*pi);
		}

		UpdateOrientation();

		// Handle zoom
		if (m_mbuttonCur == MBUTTON_Right)
		{
			m_radius *= expf(mouseMove.y * m_zoomSpeed);
		}
		m_radius *= expf(-m_wheelDelta * m_zoomWheelSpeed);
		m_wheelDelta = 0;

		// Handle motion of target point
		if (m_mbuttonCur == MBUTTON_Middle)
		{
			m_posTarget -= m_rotateSpeed * mouseMove.x * m_radius * m_viewToWorld.m_linear[0];
			m_posTarget += m_rotateSpeed * mouseMove.y * m_radius * m_viewToWorld.m_linear[1];
		}

		// Calculate remaining matrices
		m_pos = m_posTarget + m_radius * m_viewToWorld.m_linear[2];
		m_viewToWorld.m_translation = makefloat3(m_pos);
		UpdateWorldToClip();
	}

	void MayaCamera::LookAt(point3 posCamera, point3 posTarget)
	{
		m_posTarget = posTarget;
		m_pos = posCamera;
		m_viewToWorld.m_translation = makefloat3(m_pos);

		// Calculate angles to look at posTarget
		float3 vecToTarget = posTarget - posCamera;
		if (all(isnear(vecToTarget, 0.0f)))
		{
			assert(false);
			return;
		}
		m_radius = length(vecToTarget);
		vecToTarget /= m_radius;
		m_yaw = atan2f(-vecToTarget.z, vecToTarget.x);
		m_pitch = asinf(vecToTarget.y);

		// Update matrices
		UpdateOrientation();
		UpdateWorldToClip();
	}

	void MayaCamera::UpdateOrientation()
	{
		// Calculate new orientation matrix using spherical coordinates
		float cosYaw = cosf(m_yaw);
		float sinYaw = sinf(m_yaw);
		float cosPitch = cosf(m_pitch);
		float sinPitch = sinf(m_pitch);
		float3 forward = { cosYaw * cosPitch, sinPitch, -sinYaw * cosPitch };
		float3 right = { sinYaw, 0, cosYaw };
		float3 up = cross(right, forward);
		m_viewToWorld.m_linear = makefloat3x3(right, up, -forward);
	}
}
