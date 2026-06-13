#ifndef ASYNC_KEY_H
#define ASYNC_KEY_H

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "user32.lib")
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#else
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#endif


/**
 * @brief Utilizes the APIs for asynchronous keyboard control on Windows and Linux systems and provides a unified, system-independent adapter.
 *
 * @author Ibrahim Ibram
 * @date June 2026
 */
namespace AsyncKey
{
	enum class Key
	{
		// Pfeiltasten & System
		UP,
		DOWN,
		LEFT,
		RIGHT,
		ESC,
		ENTER,
		SPACE,
		TAB,

		// WASD-Steuerung
		W,
		A,
		S,
		D
	};

	// ================================================================================================================
	// WINDOWS IMPLEMENTIERUNG (Inline)
	// ================================================================================================================
	#if defined(_WIN32) || defined(_WIN64)

	inline bool isPressed(Key key)
	{
		int vKey = 0;
		switch (key)
		{
			case Key::UP:    vKey = 0x26; break; // VK_UP
			case Key::DOWN:  vKey = 0x28; break; // VK_DOWN
			case Key::LEFT:  vKey = 0x25; break; // VK_LEFT
			case Key::RIGHT: vKey = 0x27; break; // VK_RIGHT
			case Key::ESC:   vKey = 0x1B; break; // VK_ESCAPE
			case Key::ENTER: vKey = 0x0D; break; // VK_RETURN
			case Key::SPACE: vKey = 0x20; break; // VK_SPACE
			case Key::TAB:   vKey = 0x09; break; // VK_TAB
			case Key::W:     vKey = 0x57; break; // 'W'
			case Key::A:     vKey = 0x41; break; // 'A'
			case Key::S:     vKey = 0x53; break; // 'S'
			case Key::D:     vKey = 0x44; break; // 'D'
			default: return false;
		}
		return (GetAsyncKeyState(vKey) & 0x8000) != 0;
	}

	// ================================================================================================================
	// LINUX IMPLEMENTIERUNG (Inline)
	// ================================================================================================================
	#else

	inline bool checkLinuxKeyState(int linuxKeyCode)
	{
		// Statischer File-Descriptor: Bleibt über die gesamte Programmlaufzeit offen
		static int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
		if (fd < 0) return false;

		unsigned char key_b[(KEY_MAX + 7) / 8];

		if (ioctl(fd, EVIOCGKEY(sizeof(key_b)), key_b) < 0)
		{
			return false;
		}

		int byte_nr = linuxKeyCode / 8;
		int bit_nr = linuxKeyCode % 8;
		return (key_b[byte_nr] & (1 << bit_nr)) != 0;
	}

	inline bool isPressed(Key key)
	{
		int linuxKey = 0;
		switch (key)
		{
			case Key::UP:    linuxKey = KEY_UP;    break;
			case Key::DOWN:  linuxKey = KEY_DOWN;  break;
			case Key::LEFT:  linuxKey = KEY_LEFT;  break;
			case Key::RIGHT: linuxKey = KEY_RIGHT; break;
			case Key::ESC:   linuxKey = KEY_ESC;   break;
			case Key::ENTER: linuxKey = KEY_ENTER; break;
			case Key::SPACE: linuxKey = KEY_SPACE; break;
			case Key::TAB:   linuxKey = KEY_TAB;   break;
			case Key::W:     linuxKey = KEY_W;     break;
			case Key::A:     linuxKey = KEY_A;     break;
			case Key::S:     linuxKey = KEY_S;     break;
			case Key::D:     linuxKey = KEY_D;     break;
			default: return false;
		}
		return checkLinuxKeyState(linuxKey);
	}

	#endif
}
#endif // ASYNC_KEY_H
