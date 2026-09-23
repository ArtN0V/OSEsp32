local fs = osesp32.fs

local function show_welcome()
  local handle, err = fs.open("app:/welcome.txt", "r")
  if not handle then
    osesp32.ui.label(err)
    return
  end
  local message
  message, err = fs.read(handle, 96)
  fs.close(handle)
  osesp32.ui.label(message or err)
end

function main()
  osesp32.ui.button(1, "Show resource")
  osesp32.ui.button(6, "Exit")
  show_welcome()
  while true do
    local event = osesp32.ui.wait()
    if event == 1 then show_welcome() end
    if event == 6 then osesp32.exit() end
  end
end
