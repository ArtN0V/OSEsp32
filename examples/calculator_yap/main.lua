local ui = osesp32.ui

local captions = {
  "C", "+/-", "%", "/",
  "7", "8", "9", "*",
  "4", "5", "6", "-",
  "1", "2", "3", "+",
  "Exit", "0", ".", "="
}

local function number_text(value)
  if value == math.floor(value) then return tostring(math.floor(value)) end
  local text = tostring(value)
  if #text > 14 then return "Error" end
  return text
end

local function has_dot(text)
  for index = 1, #text do
    if text:sub(index, index) == "." then return true end
  end
  return false
end

function main()
  local display, stored, operation, fresh = "0", nil, nil, true

  local function show()
    ui.label(100, display, 8, 6, 304, 34, "status")
  end

  local function calculate()
    if not stored or not operation then return end
    local current = tonumber(display)
    if not current or (operation == "/" and current == 0) then
      display, stored, operation, fresh = "Error", nil, nil, true
      return
    end
    if operation == "+" then stored = stored + current
    elseif operation == "-" then stored = stored - current
    elseif operation == "*" then stored = stored * current
    else stored = stored / current end
    display, operation, fresh = number_text(stored), nil, true
  end

  ui.clear()
  show()
  for index, caption in ipairs(captions) do
    local column = (index - 1) % 4
    local row = math.floor((index - 1) / 4)
    ui.button(index, caption, 8 + column * 78, 44 + row * 38, 70, 32)
  end

  while true do
    local id, event = ui.wait()
    if event == "tap" then
      local key = captions[id]
      if key == "Exit" then
        osesp32.exit()
      elseif key == "C" then
        display, stored, operation, fresh = "0", nil, nil, true
      elseif key == "+/-" then
        if display ~= "0" and display ~= "Error" then
          display = display:sub(1, 1) == "-" and display:sub(2) or ("-" .. display)
        end
      elseif key == "%" then
        local value = tonumber(display)
        display, fresh = value and number_text(value / 100) or "Error", true
      elseif key == "=" then
        calculate()
      elseif key == "+" or key == "-" or key == "*" or key == "/" then
        local value = tonumber(display)
        if value then
          if stored and operation and not fresh then calculate() else stored = value end
          operation, fresh = key, true
        end
      elseif key == "." then
        if fresh or display == "Error" then display, fresh = "0.", false
        elseif not has_dot(display) then display = display .. "." end
      else
        if fresh or display == "0" or display == "Error" then display, fresh = key, false
        elseif #display < 14 then display = display .. key end
      end
      show()
    end
  end
end
