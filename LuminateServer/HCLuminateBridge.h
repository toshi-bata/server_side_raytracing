#pragma once

#include <string>

#include <RED.h>
#include <REDObject.h>
#include <REDIWindow.h>

/**
 * Structure storing the frame statistics.
 */
struct FrameStatistics {
	bool renderingIsDone;
	float renderingProgress;
	float remainingTimeMilliseconds;
	int numberOfPasses;
	int currentPass;
};

class HCLuminateBridge
{
public:
	HCLuminateBridge();
	~HCLuminateBridge();

public:
	/**
	* Request to draw a new frame.
	* This method must be called continuously and will check by itself
	* if cameras must be sync and if a new render must be processed.
	* @return True if success, otherwise False.
	*/
	bool draw();

	/**
	 * Shutdown completely Luminate.
	 * This will destroy and free everything.
	 * @return True if success, otherwise False.
	 */
	bool shutdown();

	/**
	 * Resize the Luminate window.
	 * @param[in] a_windowWidth New width to set.
	 * @param[in] a_windowHeight New height to set.
	 * @return True if success, otherwise False.
	 */
	bool resize(int a_windowWidth, int a_windowHeight);

	bool initialize(std::string const& a_license, void* a_osHandle, int a_windowWidth, int a_windowHeight);
	bool saveImg();

protected:
	// Window.
	RED::Object* m_window;
	int m_windowWidth;
	int m_windowHeight;

	// Offscreen window
	RED::Object* m_auxvrl;
	RED::Object* m_auxcamera;

	// Render
	bool m_frameIsComplete;
	bool m_newFrameIsRequired;
	FrameStatistics m_lastFrameStatistics;
	RED::FRAME_TRACING_FEEDBACK m_frameTracingMode;

private:
	static RED_RC createCamera(RED::Object* a_window, int a_windowWidh, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera);
	RED_RC syncLuminateCamera();

};

RED_RC setLicense(char const* a_license, bool& a_outLicenseIsActive);
RED_RC setSoftTracerMode(int a_mode);
RED_RC createRedWindow(void* a_osHandler,
	int a_width,
	int a_height,
#ifdef _LIN32
	Display* a_display,
	int a_screen,
	Visual* a_visual,
#endif
	RED::Object*& a_outWindow);

/**
 * Stops window tracing and shutdown completely Luminate.
 * This will destroy and free everything.
 * @return True if success, otherwise False.
 */
RED_RC shutdownLuminate(RED::Object* a_window);

/**
 * Create a new camera attached to a window.
 * @param[in] a_window Window on which to attach the new camera.
 * @param[in] a_windowWidth Current window width.
 * @param[in] a_windowHeight Current window height.
 * @param[out] a_outCamera Output camera.
 * @return RED_OK if success, otherwise error code.
 */
RED_RC createRedCamera(RED::Object* a_window, int a_windowWidth, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera);

/**
 * Resize the Luminate window.
 * @param[in] a_windowWidth New width to set.
 * @param[in] a_windowHeight New height to set.
 * @return RED_OK if success, otherwise error code.
 */
RED_RC resizeWindow(RED::Object* a_window, int a_newWidth, int a_newHeight);

RED_RC checkDrawTracing(RED::Object* a_window,
	RED::FRAME_TRACING_FEEDBACK a_tracingMode,
	bool& a_ioFrameIsComplete,
	bool& a_ioNewFrameIsRequired);

RED_RC checkDrawHardware(RED::Object* a_window);

RED_RC checkFrameStatistics(RED::Object* a_window, FrameStatistics* a_stats, bool& a_ioFrameIsComplete);
