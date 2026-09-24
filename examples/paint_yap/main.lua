local ui = osesp32.ui
local canvas = osesp32.canvas

local palette = {0, 1, 2, 3, 4, 5, 6, 15}
local paletteNames = {"BK", "RD", "GN", "BL", "YL", "MG", "CY", "WH"}

local color = 0
local thickness = 3
local eraser = false
local dirty = false
local drawing = false
local previousX, previousY = 0, 0

local function refreshTools()
  ui.button(1, eraser and "PEN" or "PEN>", 2, 178, 58, 28)
  ui.button(2, eraser and "ERASE>" or "ERASE", 64, 178, 58, 28)
  ui.button(3, "SIZE " .. thickness, 126, 178, 58, 28)
  ui.button(4, "CLEAR", 188, 178, 58, 28)
  ui.button(5, dirty and "EXIT*" or "EXIT", 250, 178, 68, 28)
end

local function setDirty(value)
  if dirty ~= value then
    dirty = value
    refreshTools()
  end
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
      eraser = false
      refreshTools()
    elseif event == "tap" and id == 2 then
      eraser = true
      refreshTools()
    elseif event == "tap" and id == 3 then
      thickness = thickness + 2
      if thickness > 9 then thickness = 1 end
      refreshTools()
    elseif event == "tap" and id == 4 then
      if not dirty or ui.confirm("Clear the drawing?") then
        assert(canvas.clear(15))
        setDirty(false)
      end
    elseif event == "tap" and id == 5 then
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
