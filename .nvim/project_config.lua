-- Config file for starting a work environment on IronAge.io

-- Load base config present for user.
dofile(vim.fn.stdpath('config') .. "/init.lua")

-- Begin project-specific config.
print("Loading Config for project IronAge.")

-- Set $MYVIMRC to point to this file.
vim.env.MYVIMRC = debug.getinfo(1, "S").source:sub(2)

-- Config functions, called at the end of the file if IronAge dev env was correctly setup.

local run_command_term = function(cmd, auto_close, switch_modes)
    print("Running full build...")

    local buf = vim.api.nvim_create_buf(false, true)
    vim.cmd("rightbelow vsplit")
    if (switch_modes) then
        vim.cmd("startinsert")
    end
    local win = vim.api.nvim_get_current_win()
    vim.api.nvim_win_set_buf(win, buf)
    vim.keymap.set({ "n", "t" }, "<CR>", "<cmd>bdelete!<CR>", { buffer = buf }) -- Make it so pressing <CR> in the buffer closes it.

    local jobID = vim.api.nvim_buf_call(buf, function ()
        return vim.fn.jobstart(cmd, {
            term = true,
            on_exit = function()
                local shouldClose = (auto_close == nil) or auto_close
                if (shouldClose) and vim.api.nvim_win_is_valid(win) then
                    vim.api.nvim_win_close(win, true)
                end
            end,
        })
    end)

    return jobID, buf
end

local GAME_SERVER_BUILD_CMD = vim.fn.getenv("APP_BUILD_WIN32_GAME_SERVER")
local game_server_build = function()
    run_command_term(GAME_SERVER_BUILD_CMD, false, false)
end

local GAME_SERVER_BUILD_HOTRELOAD_CMD = vim.fn.getenv("PROJECT_ROOT") .. "scripts/win32_build_game_server_dll.cmd"
local game_server_build_hotreload = function()
    vim.cmd("silent !" .. GAME_SERVER_BUILD_HOTRELOAD_CMD, true, false)
end

local TEST_CMD = vim.fn.getenv("PROJECT_ROOT") .. "scripts/win32_launch_server.cmd"
local test = function ()
    run_command_term(TEST_CMD, false, true)
end

local build_and_test = function ()
    run_command_term(GAME_SERVER_BUILD_CMD .. "&" .. TEST_CMD, false, true)
end

local FULL_BUILD_CMD = vim.fn.getenv("PROJECT_ROOT") .. "scripts/win32_build_all.cmd"
local full_build = function()
    run_command_term(FULL_BUILD_CMD, false, true)
end

local full_build_and_test = function ()
    run_command_term(FULL_BUILD_CMD .. "&" .. TEST_CMD, false, true)
end

local setup_key_mappings = function()
    vim.keymap.set("n", "<leader>b", game_server_build) -- @TODO(Marc): Target system, toggling between active targets (Game server platform, game server dll, client...).
    vim.keymap.set("n", "<leader>bb", game_server_build_hotreload) -- @TODO(Marc): Target system, toggling between active targets (Game server platform, game server dll, client...).
    vim.keymap.set("n", "<leader>t", test) -- @TODO(Marc): Same target system, launching the correct app.
    vim.keymap.set("n", "<leader>bt", build_and_test) -- @TODO(Marc): IDEM
    vim.keymap.set("n", "<leader>B", full_build)
    vim.keymap.set("n", "<leader>T", full_build_and_test)
end

-- Project assumes you've called dev_env_setup.cmd
if vim.fn.getenv("IRONAGE_DEV_SETUP") == vim.v.null then
    print("IronAge dev setup not ran. Call <Project Root>/scripts/dev_env_setup.cmd before loading this config.")
else
    setup_key_mappings()
end

