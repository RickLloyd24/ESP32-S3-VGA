//============================================================
// USB.cpp - USB Host Keyboard Driver for ESP32-S3
// Uses ESP-IDF USB Host Library with HID boot protocol
//============================================================
#include <Arduino.h>
#include "USB.h"
#include "usb/usb_host.h"

//============ Ring Buffer ================
#define KEY_BUF_SIZE 64
static volatile char keyBuf[KEY_BUF_SIZE];
static volatile int bufHead = 0;
static volatile int bufTail = 0;

static void bufPut(char c) {
	int next = (bufHead + 1) % KEY_BUF_SIZE;
	if (next != bufTail) {
		keyBuf[bufHead] = c;
		bufHead = next;
	}
}

//============ HID Scancode to ASCII (US Layout) ================
// Index = HID Usage ID, Value = ASCII character
static const char SCANCODE[57] = {
	0, 0, 0, 0,                                            // 0x00-0x03: reserved
	'a','b','c','d','e','f','g','h','i','j',                // 0x04-0x0D
	'k','l','m','n','o','p','q','r','s','t',                // 0x0E-0x17
	'u','v','w','x','y','z',                                // 0x18-0x1D
	'1','2','3','4','5','6','7','8','9','0',                // 0x1E-0x27
	'\n', 0x1B, '\b', '\t', ' ',                            // 0x28-0x2C: Enter,Esc,BS,Tab,Space
	'-', '=', '[', ']', '\\', '#', ';', '\'', '`',         // 0x2D-0x35
	',', '.', '/'                                           // 0x36-0x38
};

static const char SCANCODE_SHIFT[57] = {
	0, 0, 0, 0,
	'A','B','C','D','E','F','G','H','I','J',
	'K','L','M','N','O','P','Q','R','S','T',
	'U','V','W','X','Y','Z',
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	'\n', 0x1B, '\b', '\t', ' ',
	'_', '+', '{', '}', '|', '~', ':', '"', '~',
	'<', '>', '?'
};

//============ USB State ================
static usb_host_client_handle_t clientHdl = NULL;
static usb_device_handle_t devHdl = NULL;
static usb_transfer_t *xferIn = NULL;
static uint8_t hidItfNum = 0;
static uint8_t prevKeys[6] = {0};
static volatile uint8_t pendingAddr = 0;
static volatile bool devDisconnected = false;

//============ Keyboard Report Processing ================
static void processReport(uint8_t *data, int len) {
	if (len < 8) return;
	uint8_t mod = data[0];
	bool shift = (mod & 0x22) != 0;  // Left Shift (bit1) or Right Shift (bit5)

	for (int i = 2; i < 8; i++) {
		uint8_t code = data[i];
		if (code == 0 || code >= 57) continue;

		// Skip keys already held in previous report (not new presses)
		bool held = false;
		for (int j = 0; j < 6; j++) {
			if (prevKeys[j] == code) { held = true; break; }
		}
		if (held) continue;

		char c = shift ? SCANCODE_SHIFT[code] : SCANCODE[code];
		if (c) bufPut(c);
	}
	memcpy(prevKeys, &data[2], 6);
}

//============ Transfer Callbacks ================
static void onTransferComplete(usb_transfer_t *xfer) {
	if (xfer->status == 0 && xfer->actual_num_bytes >= 8) {
		processReport(xfer->data_buffer, xfer->actual_num_bytes);
	}
	// Resubmit if device still connected
	if (devHdl && !devDisconnected) {
		usb_host_transfer_submit(xfer);
	}
}

static void onCtrlComplete(usb_transfer_t *xfer) {
	// Control transfer done
}

//============ Client Event Callback ================
static void onClientEvent(const usb_host_client_event_msg_t *msg, void *arg) {
	switch (msg->event) {
		case USB_HOST_CLIENT_EVENT_NEW_DEV:
			pendingAddr = msg->new_dev.address;
			break;
		case USB_HOST_CLIENT_EVENT_DEV_GONE:
			devDisconnected = true;
			break;
	}
}

//============ Device Setup & Cleanup ================
static bool openKeyboard(uint8_t addr) {
	esp_err_t err = usb_host_device_open(clientHdl, addr, &devHdl);
	if (err != ESP_OK) {
		Serial.printf("USB: device open failed (addr %d): %s\n", addr, esp_err_to_name(err));
		return false;
	}

	// Get active configuration descriptor
	const usb_config_desc_t *cfgDesc;
	err = usb_host_get_active_config_descriptor(devHdl, &cfgDesc);
	if (err != ESP_OK) {
		Serial.println("USB: get config descriptor failed");
		usb_host_device_close(clientHdl, devHdl);
		devHdl = NULL;
		return false;
	}

	// Parse descriptors to find HID boot keyboard interface + interrupt IN endpoint
	const uint8_t *p = (const uint8_t *)cfgDesc;
	int offset = 0;
	int total = cfgDesc->wTotalLength;
	uint8_t epAddr = 0;
	bool foundItf = false;

	while (offset < total) {
		const usb_standard_desc_t *d = (const usb_standard_desc_t *)(p + offset);
		if (d->bLength == 0) break;

		if (d->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
			const usb_intf_desc_t *itf = (const usb_intf_desc_t *)d;
			// HID class=3, Boot subclass=1, Keyboard protocol=1
			foundItf = (itf->bInterfaceClass == 3 &&
			            itf->bInterfaceSubClass == 1 &&
			            itf->bInterfaceProtocol == 1);
			if (foundItf) {
				hidItfNum = itf->bInterfaceNumber;
				Serial.printf("USB: HID boot keyboard on interface %d\n", hidItfNum);
			}
		} else if (foundItf && d->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
			const usb_ep_desc_t *ep = (const usb_ep_desc_t *)d;
			if ((ep->bEndpointAddress & 0x80) &&       // IN direction
			    (ep->bmAttributes & 0x03) == 0x03) {   // Interrupt type
				epAddr = ep->bEndpointAddress;
				Serial.printf("USB: interrupt IN endpoint 0x%02X\n", epAddr);
				break;
			}
		}
		offset += d->bLength;
	}

	if (!foundItf || epAddr == 0) {
		Serial.println("USB: not a HID boot keyboard");
		usb_host_device_close(clientHdl, devHdl);
		devHdl = NULL;
		return false;
	}

	// Claim the HID interface
	err = usb_host_interface_claim(clientHdl, devHdl, hidItfNum, 0);
	if (err != ESP_OK) {
		Serial.printf("USB: claim interface failed: %s\n", esp_err_to_name(err));
		usb_host_device_close(clientHdl, devHdl);
		devHdl = NULL;
		return false;
	}

	// SET_PROTOCOL: switch to boot protocol (simpler 8-byte reports)
	usb_transfer_t *ctrl;
	usb_host_transfer_alloc(64, 0, &ctrl);
	usb_setup_packet_t *setup = (usb_setup_packet_t *)ctrl->data_buffer;
	setup->bmRequestType = 0x21;   // Host→Device, Class, Interface
	setup->bRequest = 0x0B;        // SET_PROTOCOL
	setup->wValue = 0;             // 0 = Boot protocol
	setup->wIndex = hidItfNum;
	setup->wLength = 0;
	ctrl->num_bytes = sizeof(usb_setup_packet_t);
	ctrl->device_handle = devHdl;
	ctrl->bEndpointAddress = 0;
	ctrl->callback = onCtrlComplete;
	ctrl->context = NULL;
	usb_host_transfer_submit_control(clientHdl, ctrl);
	vTaskDelay(pdMS_TO_TICKS(100));
	usb_host_transfer_free(ctrl);

	// Allocate and submit interrupt IN transfer for keyboard reports
	usb_host_transfer_alloc(64, 0, &xferIn);
	xferIn->device_handle = devHdl;
	xferIn->bEndpointAddress = epAddr;
	xferIn->callback = onTransferComplete;
	xferIn->context = NULL;
	xferIn->num_bytes = 8;

	err = usb_host_transfer_submit(xferIn);
	if (err != ESP_OK) {
		Serial.printf("USB: submit IN transfer failed: %s\n", esp_err_to_name(err));
		usb_host_transfer_free(xferIn);
		xferIn = NULL;
		usb_host_interface_release(clientHdl, devHdl, hidItfNum);
		usb_host_device_close(clientHdl, devHdl);
		devHdl = NULL;
		return false;
	}

	Serial.println("USB: keyboard ready!");
	return true;
}

static void closeKeyboard() {
	devDisconnected = false;
	vTaskDelay(pdMS_TO_TICKS(50));  // Allow pending transfer callbacks to complete
	if (xferIn) {
		usb_host_transfer_free(xferIn);
		xferIn = NULL;
	}
	if (devHdl) {
		usb_host_interface_release(clientHdl, devHdl, hidItfNum);
		usb_host_device_close(clientHdl, devHdl);
		devHdl = NULL;
	}
	memset(prevKeys, 0, sizeof(prevKeys));
	Serial.println("USB: keyboard disconnected");
}

//============ FreeRTOS Tasks ================
static void usbHostTask(void *arg) {
	while (true) {
		usb_host_lib_handle_events(portMAX_DELAY, NULL);
	}
}

static void usbClassTask(void *arg) {
	// Register as USB host client
	usb_host_client_config_t cfg = {};
	cfg.is_synchronous = false;
	cfg.max_num_event_msg = 5;
	cfg.async.client_event_callback = onClientEvent;
	cfg.async.callback_arg = NULL;

	esp_err_t err = usb_host_client_register(&cfg, &clientHdl);
	if (err != ESP_OK) {
		Serial.printf("USB: client register failed: %s\n", esp_err_to_name(err));
		vTaskDelete(NULL);
		return;
	}

	while (true) {
		usb_host_client_handle_events(clientHdl, portMAX_DELAY);

		if (pendingAddr) {
			uint8_t addr = pendingAddr;
			pendingAddr = 0;
			openKeyboard(addr);
		}

		if (devDisconnected) {
			closeKeyboard();
		}
	}
}

//============ Public API ================
void initUSBKeyboard() {
	usb_host_config_t hostCfg = {};
	hostCfg.intr_flags = ESP_INTR_FLAG_LEVEL1;

	esp_err_t err = usb_host_install(&hostCfg);
	if (err != ESP_OK) {
		Serial.printf("USB: host install failed: %s\n", esp_err_to_name(err));
		return;
	}

	xTaskCreatePinnedToCore(usbHostTask,  "usb_host",  4096, NULL, 2, NULL, 1);
	xTaskCreatePinnedToCore(usbClassTask, "usb_class", 4096, NULL, 2, NULL, 1);

	Serial.println("USB: keyboard host initialized");
}

char getKey() {
	if (bufHead == bufTail) return 0;
	char c = keyBuf[bufTail];
	bufTail = (bufTail + 1) % KEY_BUF_SIZE;
	return c;
}
