local ui = osesp32.ui
local canvas = osesp32.canvas

local formats = {[2] = "i8", [3] = "i4"}

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

local function stress()
  local firstFree, firstBlock, lastFree, lastBlock
  for cycle = 1, 10 do
    show("I4 stress cycle " .. cycle .. "/10")
    local stats, error = canvas.probe("i4")
    if not stats then
      show("I4 stress failed " .. cycle .. ": " .. error)
      return
    end
    local released, releaseError = canvas.release()
    if not released then
      show("Release failed " .. cycle .. ": " .. releaseError)
      return
    end
    if cycle == 1 then
      firstFree = released.free_active
      firstBlock = released.largest_active
    end
    lastFree = released.free_active
    lastBlock = released.largest_active
  end
  show("I4 stress 10x OK\nfree " .. firstFree .. ">" .. lastFree ..
    "\nblock " .. firstBlock .. ">" .. lastBlock)
end

function main()
  ui.clear()
  show("Canvas memory probe\nChoose I8/I4 or run I4 10x")
  ui.button(1, "10x", 2, 208, 58, 28)
  ui.button(2, "I8", 64, 208, 58, 28)
  ui.button(3, "I4", 126, 208, 58, 28)
  ui.button(4, "FREE", 188, 208, 58, 28)
  ui.button(5, "EXIT", 250, 208, 68, 28)

  while true do
    local id, event = ui.wait()
    if event == "tap" and id == 1 then
      stress()
    elseif event == "tap" and formats[id] then
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
