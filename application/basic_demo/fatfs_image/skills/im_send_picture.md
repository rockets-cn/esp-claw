# IM Picture Return

Use this skill when the user wants the device to send an existing local image back to a chat.

## When to use
- The user asks to send back a picture, image, photo, screenshot, camera frame, or visual result.
- The target conversation is already the active Telegram, QQ, Feishu, or WeChat chat, or the user provides an explicit target `chat_id`.

## Available tools
- `list_dir`: inspect device storage and confirm the image path
- Direct IM capabilities: `tg_send_image`, `qq_send_image`, `feishu_send_image`, `wechat_send_image`

## Path guidance
- Prefer real local paths that already exist on the device.
- Common roots in this demo are `/spiffs`, `/spiffs/qq`, `/spiffs/tg`, `/spiffs/wechat`, `/spiffs/captures`, or application-managed subdirectories.
- Use `list_dir` first if the exact path is unknown.

## Sending rules
- Use this skill only for image files such as `.jpg`, `.jpeg`, `.png`, `.gif`, or `.webp`.
- Use `tg_send_image`, `qq_send_image`, `feishu_send_image`, or `wechat_send_image` directly.
- The JSON payload should include an explicit `chat_id`, `path`, and optional `caption`.
- Use `tg_send_image` for Telegram chats, `qq_send_image` for QQ chats, `feishu_send_image` for Feishu chats, and `wechat_send_image` for WeChat chats.
- Pass `caption` only when the user wants an accompanying message.

## Examples

Send an image to a Telegram chat:
```json
{
  "chat_id": "123456",
  "path": "/spiffs/captures/latest.jpg",
  "caption": "Here is the image."
}
```

Send an image to a QQ chat:
```json
{
  "chat_id": "group123",
  "path": "/spiffs/qq/capture.jpg",
  "caption": "Here is the image."
}
```

Send an image to a Feishu chat:
```json
{
  "chat_id": "ou_xxx",
  "path": "/fatfs/data/inbox/feishu/photo.jpg",
  "caption": "Here is the image."
}
```

Send an image to a WeChat chat:
```json
{
  "chat_id": "room123",
  "path": "/spiffs/wechat/capture.jpg",
  "caption": "Here is the image."
}
```

## Workflow
1. Confirm the target image exists with `list_dir` if needed.
2. Identify the target channel: Telegram, QQ, Feishu, or WeChat.
3. Build the matching capability call: `tg_send_image`, `qq_send_image`, `feishu_send_image`, or `wechat_send_image`.
4. Execute it directly with explicit `chat_id`, `path`, and optional `caption`.
5. Tell the user whether the send succeeded.

## Notes
- This skill only sends images that already exist on the device filesystem.
- If the user wants to send `.txt`, `.json`, `.log`, `.csv`, or archives, use the im_send_file skill instead.
- In this firmware variant, WeChat supports text and image send, but generic non-image file send is not exposed through the current capability surface.
