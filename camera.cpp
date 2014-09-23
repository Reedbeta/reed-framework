#include "framework.h"
#include <xinput.h>

namespace Framework
{
	// Camera implementation

	Camera::Camera()
	:	m_mbuttonCur(MBUTTON_None),
		m_wheelDelta(0),
		m_worldToClip(float4x4::identity())
	{
	}

	bool Camera::HandleWindowsMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{
		(void)lParam;

		switch (message)
		{
		case WM_LBUTTONDOWN:
			OnMouseDown(MBUTTON_Left);
			return true;

		case WM_MBUTTONDOWN:
			OnMouseDown(MBUTTON_Middle);
			return true;

		case WM_RBUTTONDOWN:
			OnMouseDown(MBUTTON_Right);
			return true;

		case WM_LBUTTONUP:
			OnMouseUp(MBUTTON_Left);
			return true;

		case WM_MBUTTONUP:
			OnMouseUp(MBUTTON_Middle);
			return true;

		case WM_RBUTTONUP:
			OnMouseUp(MBUTTON_Right);
			return true;

		case WM_MOUSEWHEEL:
			OnMouseWheel(int(short(HIWORD(wParam))));
			return true;

		default:
			return false;
		}
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
		m_controllerPresent(false),
		m_controllerMoveSpeed(2.0f),
		m_controllerRotateSpeed(2.0f),
		m_yaw(0.0f),
		m_pitch(0.0f),
		m_pos(makepoint3(0, 0, 0))
	{
		// Check for controller
		XINPUT_STATE stateDummy;
		m_controllerPresent = (XInputGetState(0, &stateDummy) == ERROR_SUCCESS);
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

		// Retrieve controller state
		XINPUT_STATE controllerState = {};
		float2 controllerLeftStick = {}, controllerRightStick = {};
		float controllerLeftTrigger = 0.0f, controllerRightTrigger = 0.0f;
		if (m_controllerPresent)
		{
			// Look out for disconnection
			if (XInputGetState(0, &controllerState) == ERROR_SUCCESS)
			{
				// Decode axes and apply dead zones

				controllerLeftTrigger = float(max(0, controllerState.Gamepad.bLeftTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) / float(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
				controllerRightTrigger = float(max(0, controllerState.Gamepad.bRightTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) / float(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

				controllerLeftStick = makefloat2(controllerState.Gamepad.sThumbLX, controllerState.Gamepad.sThumbLY);
				float lengthLeft = length(controllerLeftStick);
				if (lengthLeft > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
					controllerLeftStick = (controllerLeftStick / lengthLeft) * (lengthLeft - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / float(32768 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				else
					controllerLeftStick = makefloat2(0.0f);

				controllerRightStick = makefloat2(controllerState.Gamepad.sThumbRX, controllerState.Gamepad.sThumbRY);
				float lengthRight = length(controllerRightStick);
				if (lengthRight > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
					controllerRightStick = (controllerRightStick / lengthRight) * (lengthRight - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / float(32768 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
				else
					controllerRightStick = makefloat2(0.0f);
			}
			else
			{
				m_controllerPresent = false;
			}
		}

		// Handle controller rotation
		if (m_controllerPresent)
		{
			m_yaw -= m_controllerRotateSpeed * controllerRightStick.x * abs(controllerRightStick.x) * timestep;
			m_yaw = modPositive(m_yaw, 2.0f*pi);

			m_pitch += m_controllerRotateSpeed * controllerRightStick.y * abs(controllerRightStick.y) * timestep;
			m_pitch = clamp(m_pitch, -0.5f*pi, 0.5f*pi);
		}

		UpdateOrientation();

		// Handle translation

		// !!!UNDONE: acceleration based on how long you've been holding the button,
		// to make fine motions easier?
		float moveStep = timestep * m_moveSpeed;

		// !!!UNDONE: move keyboard tracking into an input system that respects focus, etc.
		if (GetAsyncKeyState(VK_SHIFT) || (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER))
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

		if (m_controllerPresent)
		{
			float3 localVelocity = {};
			localVelocity.x = controllerLeftStick.x * abs(controllerLeftStick.x);
			localVelocity.y = square(controllerRightTrigger) - square(controllerLeftTrigger);
			localVelocity.z = -controllerLeftStick.y * abs(controllerLeftStick.y);
			m_pos += (localVelocity * m_viewToWorld.m_linear) * (moveStep * m_controllerMoveSpeed);
		}

		// Calculate remaining matrices
		m_viewToWorld.m_translation = makefloat3(m_pos);
		UpdateWorldToClip();
	}

	void FPSCamera::LookAt(point3_arg posCamera, point3_arg posTarget)
	{
		m_pos = posCamera;
		m_viewToWorld.m_translation = makefloat3(m_pos);

		// Calculate angles to look at posTarget
		float3 vecToTarget = posTarget - posCamera;
		ASSERT_WARN(!all(isnear(vecToTarget, 0.0f)));
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
		m_controllerPresent(false),
		m_controllerMoveSpeed(2.0f),
		m_controllerZoomSpeed(2.0f),
		m_controllerRotateSpeed(2.0f),
		m_yaw(0.0f),
		m_pitch(0.0f),
		m_posTarget(makepoint3(0, 0, 0)),
		m_radius(1.0f),
		m_pos(makepoint3(-1, 0, 0))
	{
		// Check for controller
		XINPUT_STATE stateDummy;
		m_controllerPresent = (XInputGetState(0, &stateDummy) == ERROR_SUCCESS);
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

		// Retrieve controller state
		XINPUT_STATE controllerState = {};
		float2 controllerLeftStick = {}, controllerRightStick = {};
		float controllerLeftTrigger = 0.0f, controllerRightTrigger = 0.0f;
		if (m_controllerPresent)
		{
			// Look out for disconnection
			if (XInputGetState(0, &controllerState) == ERROR_SUCCESS)
			{
				// Decode axes and apply dead zones

				controllerLeftTrigger = float(max(0, controllerState.Gamepad.bLeftTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) / float(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
				controllerRightTrigger = float(max(0, controllerState.Gamepad.bRightTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) / float(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

				controllerLeftStick = makefloat2(controllerState.Gamepad.sThumbLX, controllerState.Gamepad.sThumbLY);
				float lengthLeft = length(controllerLeftStick);
				if (lengthLeft > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
					controllerLeftStick = (controllerLeftStick / lengthLeft) * (lengthLeft - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / float(32768 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				else
					controllerLeftStick = makefloat2(0.0f);

				controllerRightStick = makefloat2(controllerState.Gamepad.sThumbRX, controllerState.Gamepad.sThumbRY);
				float lengthRight = length(controllerRightStick);
				if (lengthRight > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
					controllerRightStick = (controllerRightStick / lengthRight) * (lengthRight - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / float(32768 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
				else
					controllerRightStick = makefloat2(0.0f);
			}
			else
			{
				m_controllerPresent = false;
			}
		}

		// Handle controller rotation
		if (m_controllerPresent)
		{
			m_yaw -= m_controllerRotateSpeed * controllerRightStick.x * abs(controllerRightStick.x) * timestep;
			m_yaw = modPositive(m_yaw, 2.0f*pi);

			m_pitch += m_controllerRotateSpeed * controllerRightStick.y * abs(controllerRightStick.y) * timestep;
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

		// Handle controller zoom
		if (m_controllerPresent && !(controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER))
		{
			m_radius *= expf(-controllerLeftStick.y * abs(controllerLeftStick.y) * m_controllerZoomSpeed * timestep);
		}

		// Handle motion of target point
		if (m_mbuttonCur == MBUTTON_Middle)
		{
			m_posTarget -= m_rotateSpeed * mouseMove.x * m_radius * m_viewToWorld.m_linear[0];
			m_posTarget += m_rotateSpeed * mouseMove.y * m_radius * m_viewToWorld.m_linear[1];
		}

		// Handle controller motion of target point
		if (m_controllerPresent && (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER))
		{
			float3 localVelocity = {};
			localVelocity.x = controllerLeftStick.x * abs(controllerLeftStick.x);
			localVelocity.y = square(controllerRightTrigger) - square(controllerLeftTrigger);
			localVelocity.z = -controllerLeftStick.y * abs(controllerLeftStick.y);
			m_posTarget += (localVelocity * m_viewToWorld.m_linear) * (m_radius * m_controllerMoveSpeed * timestep);
		}

		// Calculate remaining matrices
		m_pos = m_posTarget + m_radius * m_viewToWorld.m_linear[2];
		m_viewToWorld.m_translation = makefloat3(m_pos);
		UpdateWorldToClip();
	}

	void MayaCamera::LookAt(point3_arg posCamera, point3_arg posTarget)
	{
		m_posTarget = posTarget;
		m_pos = posCamera;
		m_viewToWorld.m_translation = makefloat3(m_pos);

		// Calculate angles to look at posTarget
		float3 vecToTarget = posTarget - posCamera;
		ASSERT_WARN(!all(isnear(vecToTarget, 0.0f)));
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
