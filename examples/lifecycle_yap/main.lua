function main()
  local seconds = 0
  while true do
    osesp32.ui.label("YAP is running: " .. seconds .. " s\nTouch EXIT to return to desktop")
    osesp32.sleep(1000)
    seconds = seconds + 1
  end
end
