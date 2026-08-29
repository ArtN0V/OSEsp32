function main()
  local values = {}
  while true do
    values[#values + 1] = string.rep("x", 1024)
  end
end
