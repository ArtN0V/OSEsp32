local ui = osesp32.ui
local canvas = osesp32.canvas

local formats = {[1] = "rgb565", [2] = "i8", [3] = "i4"}

local function show(text)
  ui.label(100, text:sub(1, 96), 4, 4, 312, 58, "status")
end

local function describe(stats)
  return stats.format .. "  " .. stats.buffer_bytes .. " B" ..
    "\nfree " .. stats.free_active .. "  block " .. stats.largest_active ..
    "  min " .. stats.minimum_free ..
    "\nalloc/fill " .. stats.allocation_us .. "/" .. stats.fill_us ..
    "us  fr " .. stats.average_frame_ms .. "/" .. stats.maximum_frame_ms .. "ms"
end

function main()
  ui.clear()
  show("Canvas memory probe\nChoose RGB565, I8 or I4")
  ui.button(1, "RGB", 2, 208, 58, 28)
  ui.button(2, "I8", 64, 208, 58, 28)
  ui.button(3, "I4", 126, 208, 58, 28)
  ui.button(4, "FREE", 188, 208, 58, 28)
  ui.button(5, "EXIT", 250, 208, 68, 28)

  while true do
    local id, event = ui.wait()
    if event == "tap" and formats[id] then
      show("Allocating " .. formats[id] .. " and measuring 30 frames...")
      local stats, error = canvas.probe(formats[id])
      show(stats and describe(stats) or (formats[id] .. ": " .. error))
    elseif event == "tap" and id == 4 then
      local stats = canvas.release()
      show("Released\nfree " .. stats.free_active ..
        "  block " .. stats.largest_active)
    elseif event == "tap" and id == 5 then
      canvas.release()
      osesp32.exit()
    end
  end
end
