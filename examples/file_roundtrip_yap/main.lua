local fs = osesp32.fs
local text = "Hello from OSEsp32"

local function preview(handle)
  local data, err = fs.read(handle, 96)
  fs.close(handle, false)
  if data then
    -- BMP files can be selected to test association conflicts; do not interpret
    -- arbitrary binary bytes as LVGL text.
    if data:sub(1, 2) == "BM" then osesp32.ui.label("BMP opened (Paint comes in Stage 5)")
    elseif utf8.len(data) then
      for i = 1, #data do
        local byte = data:byte(i)
        if byte == 0 or (byte < 32 and byte ~= 10 and byte ~= 13 and byte ~= 9) then
          osesp32.ui.label("Binary document opened"); return
        end
      end
      text = data; osesp32.ui.label(text)
    else osesp32.ui.label("Preview ends inside UTF-8 or is binary")
    end
  else osesp32.ui.label(err) end
end

function main()
  local r = assert(fs.open("app:/welcome.txt", "r"))
  osesp32.ui.label(assert(fs.read(r, 96)))
  assert(fs.close(r))
  local h = assert(fs.open("data:/roundtrip.txt", "w"))
  assert(fs.write(h, text)); assert(fs.close(h))
  h = assert(fs.open("data:/roundtrip.txt", "r"))
  assert(fs.read(h, 512) == text); assert(fs.close(h))
  osesp32.ui.button(1, "Edit text")
  osesp32.ui.button(2, "Open document")
  osesp32.ui.button(3, "Save as")
  osesp32.ui.button(4, "Private round trip")
  osesp32.ui.button(6, "Exit")
  local initial = osesp32.documents.current()
  if initial then preview(initial) end
  while true do
    local action = osesp32.ui.wait()
    if action == 1 then
      local initial = text:sub(1, 96)
      while not utf8.len(initial) do initial = initial:sub(1, -2) end
      local value, err = osesp32.ui.text(initial)
      if value then text = value; osesp32.ui.label("Text updated")
      else osesp32.ui.label(err) end
    elseif action == 2 then
      local selected, err = osesp32.documents.open()
      if selected then preview(selected) else osesp32.ui.label(err) end
    elseif action == 3 then
      local selected, err = osesp32.documents.save("example.txt")
      if selected then
        local ok; ok, err = fs.write(selected, text)
        if ok then ok, err = fs.close(selected) else fs.close(selected, false) end
        osesp32.ui.label(ok and "Saved" or err)
      else osesp32.ui.label(err) end
    elseif action == 4 then
      local selected, err = fs.open("data:/roundtrip.txt", "w")
      if selected then
        local ok; ok, err = fs.write(selected, text)
        if ok then ok, err = fs.close(selected) else fs.close(selected, false) end
        osesp32.ui.label(ok and "Private data saved" or err)
      else osesp32.ui.label(err) end
    elseif action == 6 then osesp32.exit() end
  end
end
