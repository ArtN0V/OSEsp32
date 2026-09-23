function main()
  local seconds = 0
  while seconds < 10 do
    osesp32.ui.label("App exit in " .. (10 - seconds) .. " s\nRecovery: hold top-left corner for 2 s")
    osesp32.sleep(1000)
    seconds = seconds + 1
  end
  osesp32.exit()
end
