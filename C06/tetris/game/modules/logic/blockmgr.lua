local render = require("modules.core.render")
local object = require("modules.core.object")
local board_class = require("modules.logic.board")
local lshape_class = require("modules.logic.block.lshape")
local square_class = require("modules.logic.block.square")
local stick_class = require("modules.logic.block.stick")
local tshape_class = require("modules.logic.block.tshape")
local zshape_class = require("modules.logic.block.zshape")
local const = require("modules.const")

local downward_gap_by_millisecond = 1000 
local duration = 0

local blockmgr = object:inherit()

function blockmgr:init()
	self.board = board_class:new()

	self:next_turn()	
	render.log("blockmgr|init|success")
end

function blockmgr:next_turn()
	if self.next_shape then 		
		self.current_shape = self.next_shape
	else
		self.current_shape = self:create_block()
		self.current_shape:random_rotate()
	end
	self.current_shape:update_center(5, 1)

	self.next_shape = self:create_block()
	self.next_shape:update_center(14, 6)
	self.next_shape:random_rotate()
end

function blockmgr:create_block()
	local block_type = rand(const.BLOCK_TYPE.TOTAL)
	if block_type == const.BLOCK_TYPE.LSHAPE then 
		return lshape_class:new()
	elseif block_type == const.BLOCK_TYPE.SQUARE then 
		return square_class:new()
	elseif block_type == const.BLOCK_TYPE.STICK then 
		return stick_class:new()
	elseif block_type == const.BLOCK_TYPE.TSHAPE then 
		return tshape_class:new()
	else
		return zshape_class:new()
	end
end

function blockmgr:reset()
	self.board:reset()
	self:next_turn()
end

function blockmgr:draw()
	render.draw_text(650, 0, "next block:")
	self.board:draw()
	self.current_shape:draw()
	self.next_shape:draw()
end

function blockmgr:key_event(event, for_ui_data)
	local center_x, center_y = self.current_shape:get_center()
	local vertexes = self.current_shape:get_vertexes()
	local boarder_pos = self.current_shape:get_border_pos(vertexes)
	local is_rotate = false

	if event == const.KEY_EVENT.MOVE_LEFT then 
		center_x = center_x - 1
	elseif event == const.KEY_EVENT.MOVE_RIGHT then 
		center_x = center_x + 1
	elseif event == const.KEY_EVENT.MOVE_UP then 
		if self.current_shape:get_type() ~= const.BLOCK_TYPE.SQUARE then 
			vertexes = self.current_shape:gen_rotate90_vertexes()
			boarder_pos = self.current_shape:get_border_pos(vertexes)
			is_rotate = true
		end
	elseif event == const.KEY_EVENT.MOVE_DOWN then 
		center_y = center_y + 1
	end	

	local center = self.board:try_fix(center_x, center_y, vertexes, boarder_pos)
	if center then 
		self.current_shape:update_center(center.x, center.y)
		if is_rotate then 
			self.current_shape:set_vertexes(vertexes)
		end
	end

	local erase_count = self:try_occupy()
	for_ui_data.erase_count = for_ui_data.erase_count + erase_count
end

function blockmgr:try_occupy()
	local center = self.current_shape:get_center_world_pos()
	local vertexes = self.current_shape:get_local_pos()

	if self.board:can_occupy(center.x, center.y, vertexes) then 
		local erase_count = self.board:occupy(center.x, center.y, vertexes, self.current_shape:get_color())
		self:next_turn()

		return erase_count
	else
		return 0
	end	
end

function blockmgr:update(delta, for_ui_data)
	if duration >= downward_gap_by_millisecond then 
		self.current_shape:move_down(1)
		local erase_count = self:try_occupy()

		for_ui_data.erase_count = for_ui_data.erase_count + erase_count
		duration = 0
	end
	duration = duration + delta
end

return blockmgr