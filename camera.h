#pragma once

namespace Framework
{
	// !!!UNDONE: break out mouse stuff into a separate input framework
	enum MBUTTON
	{
		MBUTTON_None,
		MBUTTON_Left,
		MBUTTON_Middle,
		MBUTTON_Right,
	};

	// Generic camera implementation that can be filled in
	class Camera
	{
	public:
							Camera();
		virtual				~Camera() {}

		void				OnMouseDown(MBUTTON mbutton)
								{ m_mbuttonCur = mbutton; }
		void				OnMouseUp(MBUTTON mbutton)
								{ if (mbutton == m_mbuttonCur) m_mbuttonCur = MBUTTON_None; }
		void				OnMouseWheel(int wheelDelta)
								{ m_wheelDelta += wheelDelta; }
		bool				HandleWindowsMessage(UINT message, WPARAM wParam, LPARAM lParam);

		virtual void		Update(float timestep) = 0;

		MBUTTON				m_mbuttonCur;
		int					m_wheelDelta;
	};

	// Perspective camera - implements a perspective projection matrix
	class PerspectiveCamera : public Camera
	{
	public:
		typedef Camera super;

							PerspectiveCamera();

		void				SetProjection(
								float vFOV,
								float aspect,
								float zNear,
								float zFar);

		affine3				m_viewToWorld;
		affine3				m_worldToView;
		float4x4			m_projection;
		float4x4			m_worldToClip;

		void				UpdateWorldToClip();
	};

	// FPS-style camera with WSAD controls (Y-up convention)
	class FPSCamera : public PerspectiveCamera
	{
	public:
		typedef PerspectiveCamera super;

							FPSCamera();

		virtual void		Update(float timestep);

		void				LookAt(point3_arg posCamera, point3_arg posTarget);
		void				SetPose(point3_arg posCamera, float yaw, float pitch);

		float				m_moveSpeed;		// Movement speed in units/second
		float				m_rotateSpeed;		// Mouse sensitivity in radians/pixel
		MBUTTON				m_mbuttonActivate;	// Which mouse button enables rotation?
		ipoint2				m_mousePosPrev;

		bool				m_controllerPresent;
		float				m_controllerMoveSpeed;
		float				m_controllerRotateSpeed;

		float				m_yaw;				// Yaw from +X toward +Z axis, in radians
		float				m_pitch;			// Pitch from XZ-plane toward +Y, in radians
		point3				m_pos;

		void				UpdateOrientation();
	};

	// Maya-style orbiting camera (Y-up convention)
	class MayaCamera : public PerspectiveCamera
	{
	public:
		typedef PerspectiveCamera super;

							MayaCamera();

		virtual void		Update(float timestep);

		void				LookAt(point3_arg posCamera, point3_arg posTarget);
		void				SetPose(point3_arg posTarget, float yaw, float pitch, float radius);

		float				m_rotateSpeed;		// Mouse sensitivity in radians/pixel
		float				m_zoomSpeed;		// Mouse zoom speed in nepers/pixel
		float				m_zoomWheelSpeed;	// Mouse zoom speed in nepers/wheel-tick
		ipoint2				m_mousePosPrev;

		bool				m_controllerPresent;
		float				m_controllerMoveSpeed;
		float				m_controllerZoomSpeed;
		float				m_controllerRotateSpeed;

		float				m_yaw;				// Yaw from +X toward -Z axis, in radians
		float				m_pitch;			// Pitch from XZ-plane toward +Y, in radians
		point3				m_posTarget;		// Position around which we're orbiting
		float				m_radius;			// Orbital radius
		point3				m_pos;				// Position of camera itself

		void				UpdateOrientation();
	};

	// 2D camera with mouse translation and zooming, but no rotation
	class TwoDCamera : public Camera
	{
	public:
		typedef Camera super;

							TwoDCamera();

		virtual void		Update(float timestep);

		void				FrameBox(box2_arg box)
							{
								m_pos = box.center();
								float2 diagonal = box.diagonal();
								diagonal.x *= float(m_dimsWindow.y) / float(m_dimsWindow.x);
								m_scale = maxComponent(diagonal);
								UpdateTransforms();
							}

		int2				m_dimsWindow;		// Pixel dims of window
		float				m_zoomWheelSpeed;	// Mouse zoom speed in nepers/wheel-tick
		MBUTTON				m_mbuttonActivate;	// Which mouse button enables motion?
		ipoint2				m_mousePosPrev;

		point2				m_pos;				// World position of center of screen
		float				m_scale;			// Scale from screen V [0, 1] to world space
		affine2				m_viewToWorld;		// Transform from screen UV [0, 1] to world space
		affine2				m_worldToView;		// Transform from world space to screen UV [0, 1]
		affine2				m_worldToClip;		// Transform from world space to clip space [-1, 1]

		void				UpdateTransforms();
	};
}
