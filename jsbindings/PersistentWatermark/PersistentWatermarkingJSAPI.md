# Persistent Watermarking JS Extension V0.3
## Overview
The Persistent Watermarking JS Extension supports the persistent storage of a .PNG watermark image and allows this image to be updated, displayed & hidden.

## Features
- Persistent storage of watermark & metadata
- show watermark
- hide watermark
- update watermark & metadata
- Get Metadata
- supports callback for ShowSuccess, ShowFailed, HideSuccess, HideFailed

## Expected Limitations (final version)
- 1 watermark layer only
- persistent storage for 1 .PNG image & associated metadata only
- The interface delegates all watermarking functionality to the existing, Watermarking Presentation Service, Watermark Plugin.  All limitations of this plugin apply.
- Interface is single threaded.

## Additional Limitations specific to V0.3
- All methods & data are static (i.e. if multiple instances of PersistentWatermark are created the state should be common between them & persist after the objects are garbage collected).  Notably callbacks registered using addEventListener() are not automatically removed.  Use removeEventListener() to explicitly remove them.
- removeEventListener() only the name argument is currently supported.  All registered callbacks for the named event are removed.

## Design Considerations
- The existing, Watermarking Presentation Service, Watermark Plugin is to be used for displaying watermarks.
- Persistent storage to be performed by Watermarking Presentation Service Plugin.

## Release Version History
**Version:** 0.2
**Release Notes:**
Initial Release

**Version:** 0.3
**Release Notes:**
Added persistent storage option to update.

## Abbreviation Summary
    - AAMP      Advanced Adaptive Media Player
    - JS        Javascript

## Example Use

### Show a Previously Saved Watermark
```js
	    var watermark = new PersistentWatermark();
	    watermark.show()
```

### Show a New Watermark
```js
	    var watermark = new PersistentWatermark();
	    watermark.update(NewWatermarkPNGBuffer, metadata)
	    watermark.show()
```

# Watermarking JS Extension Interface

## Methods

### update( watermarkingOverlayPNG, metadata, persistent=false)
Writes the supplied watermark & metadata to persistent storage.

Returns true if the persistent storage was updated successfully, otherwise returns false.

Calling this method will not have any effect on the display.

(Use show() to display the new watermark or hide() to hide the previously displayed watermark).

| Name | Type | Description |
| ---- | ---- | ---------- |
| watermarkingOverlayPNG | ArrayBuffer | image data in .PNG format |
| metadata | String | metadata associated with watermarkingOverlayPNG |
| persistent | bool | (optional, default false) when true, in addition to being available immediately available to display using show() the watermark will be saved to the devices file system sot that it is also available following a reboot(s) (i.e. without a call to update() following the reboot).

---

### getMetadata ()
Returns the metadata string from persistent storage.

Example:
```js
    {
    	var watermark = new PersistentWatermark();
    	console.log(watermark.getMetadata())
    }
```
---


### show ()
The watermark image is read from persistent storage and displayed.

Note: If an old watermark is currently displayed on the screen calling show() will update the displayed image.

Generates a ShowSuccess or ShowFailed event on completion.

| Name | Type | Description |
| ---- | ---- | ---------- |
| opacity | Number | Percentage multiplier for alpha (0..100). Use 100 to honor original image's alpha values.  Smaller values will make watermark more translucent down to 0 (invisible). |

Example:
```js
    {
    	var watermark = new PersistentWatermark();
    	watermark.show(75); // watermark will be rendered at 75% translucency compared to source image
    }
```
---

### hide ()
Hides any displayed watermark.

Generates a HideSuccess or HideFailed event on completion.

Example:
```js
    	var watermark = new PersistentWatermark();
	    watermark.hide();
```
---

### addEventListener( name, handler )
Registers a callback function for the specified event.
| Name | Type | Description |
| ---- | ---- | ------ |
| name | String | Event Name |
| handler | Function | Callback for processing event |

Example:
``` js
    var watermark = new PersistentWatermark();
    watermark.addEventListener("ShowSuccess", ShowSuccessFunction);;
```

---

### removeEventListener( name, handler )
Removes a previously registered callback function.
| Name | Type | Description |
| ---- | ---- | ------ |
| name | String | Event Name |
| handler | Function | Callback for processing event - not currently supported |

---

## EVENTS
Events can be subscribed/unsubscribed as required by application using addEventListener() & removeEventListener() respectively.

| Name | Description |
| ---- | ---- |
| ShowSuccess| The watermark was displayed successfully following a call to the show() method. |
| ShowFailed | The watermark could not be displayed following a call to the show() method. |
| HideSuccess| The watermark was hidden following a call to the hide() method.|
| HideFailed | The watermark was not hidden following a call to the hide() method.|
---


