# QQ File Return

Use this skill when the user wants the device to send a local file or image back to a QQ chat.

## When to use
- The user asks to send back a file, attachment, image, photo, log, or generated output through QQ.
- The target conversation is already the active QQ chat, or the user provides an explicit QQ `chat_id`.

## Available tools
- `list_dir`: inspect device storage and confirm the file path
- `read_file`: inspect small text files before sending when needed
- Direct QQ capabilities: `qq_send_image` and `qq_send_file`

## Path guidance
- Prefer real local paths already stored on the device.
- Common roots in this demo are `/spiffs`, `/spiffs/qq`, `/spiffs/lua`, or other application-managed storage paths.
- Use `list_dir` first if the exact file path is unknown.
- Use `read_file` only for small text inspection, not for binary payloads.

## Sending rules
- Use `qq_send_image` for image files such as `.jpg`, `.jpeg`, `.png`, `.gif`, or `.webp`.
- Use `qq_send_file` for non-image files such as `.txt`, `.json`, `.log`, `.csv`, or archives.
- Execute the chosen QQ capability directly.
- Pass `caption` only when the user wants an accompanying message.
- The JSON payload should include an explicit QQ `chat_id`, `path`, and optional `caption`.

## Examples

Send an image to a QQ chat:
```json
{
  "chat_id": "group123",
  "path": "/spiffs/qq/capture.jpg",
  "caption": "Here is the image."
}
```

Send a file to a QQ group:
```json
{
  "chat_id": "group1234567890",
  "path": "/spiffs/reports/status.json",
  "caption": "Latest status report."
}
```

## Workflow
1. Confirm the target file exists with `list_dir` if needed.
2. Choose `qq_send_image` or `qq_send_file` based on file type.
3. Execute the QQ capability directly with `chat_id`, `path`, and optional `caption`.
4. Tell the user whether the send succeeded.

## Notes
- This skill only sends files that already exist on the device filesystem.
- QQ generic file delivery may depend on platform-side enablement. If `qq_send_file` fails, prefer falling back to `qq_send_image` for images or explain that QQ rejected generic file upload.
