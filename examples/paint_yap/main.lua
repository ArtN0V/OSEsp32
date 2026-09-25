local ui = osesp32.ui
local canvas = osesp32.canvas
local fs = osesp32.fs
local documents = osesp32.documents

local palette = {0, 1, 2, 3, 4, 5, 6, 15}
local paletteNames = {"BK", "RD", "GN", "BL", "YL", "MG", "CY", "WH"}

local color = 0
local thickness = 3
local eraser = false
local dirty = false
local drawing = false
local previousX, previousY = 0, 0

local function refreshTools()
  ui.button(1, eraser and "ERASE>" or "PEN>", 2, 178, 51, 28)
  ui.button(2, "S" .. thickness, 55, 178, 51, 28)
  ui.button(3, "CLEAR", 108, 178, 51, 28)
  ui.button(4, "OPEN", 161, 178, 51, 28)
  ui.button(5, dirty and "SAVE*" or "SAVE", 214, 178, 51, 28)
  ui.button(6, "EXIT", 267, 178, 51, 28)
end

local function setDirty(value)
  if dirty ~= value then
    dirty = value
    refreshTools()
  end
end

local function alert(message)
  ui.confirm(message)
end

local function loadHandle(handle)
  local ok, err = canvas.load_bmp(handle)
  fs.close(handle, false)
  if ok then setDirty(false)
  else
    setDirty(true)
    alert("Open failed: " .. tostring(err))
  end
  return ok
end

local function openDrawing()
  if dirty and not ui.confirm("Discard unsaved drawing?") then return end
  local handle, err = documents.open()
  if handle then loadHandle(handle)
  elseif err ~= "cancelled" then alert("Open failed: " .. tostring(err)) end
end

local function saveDrawing()
  local handle, err = documents.save("drawing.bmp")
  if not handle then
    if err ~= "cancelled" then alert("Save failed: " .. tostring(err)) end
    return
  end
  local ok
  ok, err = canvas.save_bmp(handle)
  if ok then ok, err = fs.close(handle, true)
  else fs.close(handle, false) end
  if ok then setDirty(false) else alert("Save failed: " .. tostring(err)) end
end

local function drawTo(x, y)
  local drawColor = eraser and 15 or color
  local ok, err = canvas.line(previousX, previousY, x, y, drawColor, thickness)
  if not ok then error(err) end
  previousX, previousY = x, y
  setDirty(true)
end

function main()
  ui.clear()
  local ok, err = canvas.create(320, 176)
  if not ok then error(err) end
  assert(canvas.clear(15))
  refreshTools()
  for index = 1, #palette do
    ui.button(9 + index, paletteNames[index], 2 + (index - 1) * 39,
              210, 37, 28)
  end

  local initial = documents.current()
  if initial then loadHandle(initial) end

  while true do
    local id, event, x, y = ui.wait()
    if id == 0 and event == "canvas_down" then
      drawing = true
      previousX, previousY = x, y
      drawTo(x, y)
    elseif id == 0 and event == "canvas_move" and drawing then
      drawTo(x, y)
    elseif id == 0 and event == "canvas_up" then
      if drawing then drawTo(x, y) end
      drawing = false
    elseif event == "tap" and id == 1 then
      eraser = not eraser
      refreshTools()
    elseif event == "tap" and id == 2 then
      thickness = thickness + 2
      if thickness > 9 then thickness = 1 end
      refreshTools()
    elseif event == "tap" and id == 3 then
      if not dirty or ui.confirm("Clear the drawing?") then
        assert(canvas.clear(15))
        setDirty(true)
      end
    elseif event == "tap" and id == 4 then
      openDrawing()
    elseif event == "tap" and id == 5 then
      saveDrawing()
    elseif event == "tap" and id == 6 then
      if not dirty or ui.confirm("Exit without saving this drawing?") then
        canvas.release()
        osesp32.exit()
      end
    elseif event == "tap" and id >= 10 and id <= 17 then
      color = palette[id - 9]
      eraser = false
      refreshTools()
    end
  end
end
