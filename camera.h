#pragma once

#include <util.h>

namespace Framework
{
	using namespace util;

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

		virtual void		Update(float timestep) = 0;

		MBUTTON				m_mbuttonCur;
		int					m_wheelDelta;
		float4x4			m_worldToClip;		// Subclass should fill this in
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

	protected:
		void				UpdateWorldToClip();
	};

	// FPS-style camera with WSAD controls (Y-up convention)
	class FPSCamera : public PerspectiveCamera
	{
	public:
		typedef PerspectiveCamera super;

							FPSCamera();

		virtual void		Update(float timestep);

		void				LookAt(point3 posCamera, point3 posTarget);

		float				m_moveSpeed;		// Movement speed in units/second
		float				m_rotateSpeed;		// Mouse sensitivity in radians/pixel
		MBUTTON				m_mbuttonActivate;	// Which mouse button enables rotation?
		ipoint2				m_mousePosPrev;

		float				m_yaw;				// Yaw from +X toward +Z axis, in radians
		float				m_pitch;			// Pitch from XZ-plane toward +Y, in radians
		point3				m_pos;

	protected:
		void				UpdateOrientation();
	};

	// Maya-style orbiting camera (Y-up convention)
	class MayaCamera : public PerspectiveCamera
	{
	public:
		typedef PerspectiveCamera super;

							MayaCamera();

		virtual void		Update(float timestep);

		void				LookAt(point3 posCamera, point3 posTarget);

		float				m_rotateSpeed;		// Mouse sensitivity in radians/pixel
		float				m_zoomSpeed;		// Mouse zoom speed in nepers/pixel
		float				m_zoomWheelSpeed;	// Mouse zoom speed in nepers/wheel-tick
		ipoint2				m_mousePosPrev;

		float				m_yaw;				// Yaw from +X toward -Z axis, in radians
		float				m_pitch;			// Pitch from XZ-plane toward +Y, in radians
		point3				m_posTarget;		// Position around which we're orbiting
		float				m_radius;			// Orbital radius
		point3				m_pos;				// Position of camera itself

	protected:
		void				UpdateOrientation();
	};
}
