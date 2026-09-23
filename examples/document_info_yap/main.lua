local fs = osesp32.fs
local function inspect(h)
  local size, err = fs.size(h)
  fs.close(h)
  osesp32.ui.label(size and ("Document size: " .. size .. " bytes") or err)
end
function main()
  osesp32.ui.button(1, "Open")
  osesp32.ui.button(6, "Exit")
  local h = osesp32.documents.current()
  if h then inspect(h) else osesp32.ui.label("Select a TXT or BMP document") end
  while true do
    local id = osesp32.ui.wait()
    if id == 6 then osesp32.exit() end
    if id == 1 then
      local selected, err = osesp32.documents.open()
      if selected then inspect(selected) else osesp32.ui.label(err) end
    end
  end
end
