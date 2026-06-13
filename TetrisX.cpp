#include "TetrisX.h"


// ============================================================================================================================================================
// 													I n s t a n t i a t i o n   &   I n i t i a l i z a t i o n
// ============================================================================================================================================================

/**
 * @brief Custom Constructor: Reserves the containers and the `std::stringstream` with the expected sizes.
 * Initializes the game board and starts the main loop.
 */
TetrisX::TetrisX() : bag({ 1, 2, 3, 4, 5, 6, 7 })
{
	// reserve sufficient memory for the std::stringstream
	// (Emojis require 4 bytes in UTF-8 + any line breaks)
	uint16_t assumedSize = (Configs::SIZE + 5) * 4 + Configs::ROWS;								// + 5 lines due to the HUD
	ss.rdbuf()->str().reserve(assumedSize);

	complVec.reserve(4 * Configs::COLS);
	board.reserve(Configs::SIZE);

	setBoard();
	shuffleBag();
	startLoop();
}

/**
 * @brief Initializes the entire game board, including the edges.
 */
void TetrisX::setBoard()
{
	for (uint16_t i = 0; i < Configs::SIZE; i++)												// initialize the entire board
		board.push_back({ i, 0 });

	for (uint16_t i = 0, j = 13; i < Configs::LAST; i += Configs::COLS, j += Configs::COLS)		// process the vertical edges
	{
		board[i] = { i, 10, true };
		board[j] = { j, 10, true };
	}

	for (uint16_t i = Configs::LAST; i < Configs::SIZE; i++)									// process the bottom row
	{
		switch (i)
		{
			case Configs::LAST:
				board[i] = { i, 11, true }; break;
			case Configs::SIZE - 1:
				board[i] = { i, 13, true }; break;
			default:
				board[i] = { i, 12, true }; break;
		}
	}
}

/**
 * @brief Shuffles the cells of the bag with the "Mersenne Twister" and resets `nIDX` to the first cell
 */
void TetrisX::shuffleBag()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	std::shuffle(bag.begin(), bag.end(), gen);

	nIDX = 0;
}

// ============================================================================================================================================================
// 																	   M a i n   L o o p
// ============================================================================================================================================================

/**
 * @brief Starts the main loop and thus the next game session.
 */
void TetrisX::startLoop()
{
	std::cout << std::flush << Configs::CLEAR_DISPL << Configs::HIDE_CURSOR;		// flush and clear all characters from the console and hide the cursor

	const double FRAME_LEN = 0.02; 													// fixed update rate (1000 ms / 20 ms => 50 FPS)
	double accu = 0.0;																// accumulator for summation
	auto lastTime = std::chrono::high_resolution_clock::now();						// save current time

	nextID = bag[nIDX];																// implicitly results in: currID = bag[0] at the start of the game
	actionPerformed = true;															// implies an initial call to drawBoard()

	while (isRunning)																// the MAIN LOOP
	{
		auto currTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsedTime = currTime - lastTime;
		lastTime = currTime;

		accu += elapsedTime.count();

		while (accu >= FRAME_LEN)													// check per FRAME (every 20 ms).
		{
			accu -= FRAME_LEN;

			processInput();
			updateGame();
			drawBoard();

			++autoDown;
			++complFrame;
		}
	}

	std::cout << Configs::SHOW_CURSOR;												// show the cursor again (game over)
}

// ============================================================================================================================================================
// 																		C o n t r o l s
// ============================================================================================================================================================

/**
 * @brief Essentially the listener for user inputs (uses the AsyncKey namespace)
 */
void TetrisX::processInput()
{
	if (currID)
	{
		bool ROT = AsyncKey::isPressed(AsyncKey::Key::UP);
		bool LEFT = AsyncKey::isPressed(AsyncKey::Key::LEFT);
		bool RIGHT = AsyncKey::isPressed(AsyncKey::Key::RIGHT);

		isRunning = !(AsyncKey::isPressed(AsyncKey::Key::ESC));
		DOWN = AsyncKey::isPressed(AsyncKey::Key::DOWN);
		if (LEFT && !PREV_L)  move(Movement::LEFT);
		if (RIGHT && !PREV_R) move(Movement::RIGHT);
		if (currID != 6 && (ROT && !PREV_ROT)) rotate();						// Square (ID = 6) is not rotated! (saving resources)

		PREV_L = LEFT;
		PREV_R = RIGHT;
		PREV_ROT = ROT;
	}
}
// ============================================================================================================================================================
/**
 * @brief Controlling the game logic
 */
void TetrisX::updateGame()
{
	// if applicable: traverse the completion routine's FSM
	if (complFSM != State::IDLE)
	{
		if (complFSM == State::BOOM_1)											// starts with complFrame = 0, but sets it for the subsequent stages
		{
			completion_1(bottomRow);
			complFrame = 0;
		}

		else if (complFSM == State::BOOM_2 && complFrame == 22)
		{
			completion_2();
			complFrame = 0;
			actionPerformed = true;
		}

		else if (complFSM == State::DROP && complFrame == 15)
		{
			completion_3();
			complFSM = State::IDLE;
			actionPerformed = true;
			newBlock = true;													// completion routine finished, newBlock phase begins
		}
	}

	//else
	if (complFSM == State::IDLE)
	{
		// current block has hit a solid part or the DOWN key was just released
		// Allows continuous holding of the DOWN key without waiting for the autoDown counter to expire;
		// upon hitting the ground, releasing the DOWN key becomes mandatory (decoupling the DOWN key).
		if (hitGround || (PREV_DOWN && !DOWN))
		{
			PREV_DOWN = DOWN;
			if (hitGround)
			{
				hitGround = false;
				autoDown = 0;
			}
		}

		// If a block is in transit
		if (currID)
		{
			// drop currBlock by one row (on continuous DOWN | AUTO DOWN reached)
			if (DOWN && !PREV_DOWN || autoDown == Configs::SPEEDS[level])
			{
				move(Movement::DOWN);
				trySolidize();

				if (autoDown == Configs::SPEEDS[level])
					autoDown = 0;
			}
		}

		// if applicable: spawn a new block
		if (newBlock)
		{
			currID = nextID;

			if (nIDX == 6)
				shuffleBag();
			else
				++nIDX;

			nextID = bag[nIDX];

			currBlock = Configs::BLOCKS[currID - 1];

			for (B_Cell& b : currBlock)
				board[b.idx] = { b.idx, currID };

			autoDown = 0;
			actionPerformed = true;
			newBlock = false;
		}
	}
}

/**
 * @brief Populates the `std::stringstream` with the Unicode characters referenced by each cell's ID and inserts a newline character after each row.
 * Flushing causes the console to immediately output the contents of the `std::stringstream`. The stream is also prepared for the next frame's output.
 */
void TetrisX::drawBoard()
{
	if (actionPerformed)
	{
		// set cursor to position 0
		ss << Configs::CURSOR_POS0;

		// temporary FlatBlock
		FlatBlock nextBlock = Configs::FLAT_BLOCKS[nextID - 1];

		// ss << title bar (incl. updates)
		ss << Configs::TITLE[0] << std::format("{:0>6} \U00002503", score) << nextBlock[0];
		ss << Configs::TITLE[1] << std::format("{:0>3}    \U00002503", level) << nextBlock[1];
		ss << Configs::TITLE[2] << std::format("{:0>3}    \U00002503", lines) << nextBlock[2];
		ss << Configs::TITLE[3];

		// ss << current game board
		for (uint8_t r = 0; r < Configs::ROWS; ++r)
		{
			uint16_t base = r * Configs::COLS;

			for (uint8_t c = 0; c < Configs::COLS; ++c)
				ss << Configs::VISUALS[board[base + c].id];

			ss << '\n';
		}

		// immediately output the current game board frame to the console and reset the string stream
		std::cout << ss.str() << std::flush;
		ss.str(""); ss.clear();

		actionPerformed = false;
	}
}

// ============================================================================================================================================================
// 																	   F u n c t i o n s
// ============================================================================================================================================================

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//																		M o v e m e n t
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Captures information regarding the specified movement type and direction, stores the resulting grid indices, and provides feedback
 * on the feasibility of the intended movement direction. The function terminates early if a collision is detected.
 * @param dir enum class Movement { ROTATION, DOWN, LEFT, RIGHT }
 */
void TetrisX::setMoveInfo(Movement dir)
{
	moveInfo.canMove = false;
	short offIdx;
	short i = -1;

	if (dir == Movement::ROTATION)
	{
		for (B_Cell& b : currBlock)
		{
			// a rotation for the square (ID = 6) never actually arrives here! Listed as ROT_4X4 solely for the sake of correctness
			offIdx = (currID < 6 ? Configs::ROT_3X3[b.lvl][b.off] : Configs::ROT_4X4[b.lvl][b.off]);

			moveInfo.idx[++i] = b.idx + offIdx;
			if (board[moveInfo.idx[i]].solid)
				return;
		}
	}

	else
	{	// DOWN | LEFT | RIGHT
		if (dir == Movement::DOWN) offIdx = Configs::COLS;
		else if (dir == Movement::LEFT) offIdx = -1;
		else if (dir == Movement::RIGHT) offIdx = 1;

		for (B_Cell& b : currBlock)
		{
			moveInfo.idx[++i] = b.idx + offIdx;
			if (board[moveInfo.idx[i]].solid)
				return;
		}
	}

	moveInfo.canMove = true;
	actionPerformed = true;
}

/**
 * @brief Executes the passed movement for the current block (currBlock), if applicable
 * @param dir direction of movement -> enum class Movement { DOWN, LEFT, RIGHT } \ {ROTATION}
 */
void TetrisX::move(Movement dir)
{
	setMoveInfo(dir);

	if (moveInfo.canMove)
	{
		short i = -1;
		if (dir == Movement::LEFT)
		{
			for (B_Cell& b : currBlock)
			{
				board[b.idx].id = 0;
				b.idx = moveInfo.idx[++i];
				board[b.idx] = { b.idx, currID };
			}
		}

		else
		{	// DOWN | RIGHT		(Difference: iteration in reverse direction)
			short i = 4;
			for (B_Cell& b : currBlock | std::views::reverse)
			{
				board[b.idx].id = 0;
				b.idx = moveInfo.idx[--i];
				board[b.idx] = { b.idx, currID };
			}
		}
	}
}

/**
 * @brief Performs a right rotation of the current block.
 */
void TetrisX::rotate()
{
	setMoveInfo(Movement::ROTATION);

	if (moveInfo.canMove)
	{
		short i = -1;
		for (B_Cell& b : currBlock)
		{
			board[b.idx].id = 0;
			b.idx = moveInfo.idx[++i];
			b.off = b.off < 3 ? (b.off + 1) : 0;
		}
	}
	// now process the rotated blocks onto the board
	for (B_Cell& b : currBlock)
		board[b.idx] = { b.idx, currID };

	// sort the B_Cells in ascending order by board index
	std::ranges::sort(currBlock, {}, &B_Cell::idx);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// 															   C o m p l e t i o n   R o u t i n e
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief When `currBlock` encounters a solid block or the bottom edge of the game board during its downward movement, this function converts `currBlock`
 * into a solid block as well. It also determines the row where the block has landed; starting from this row, the system checks — moving from right to left
 * and upwards — whether and how many complete rows have been formed. This bottommost row is captured for further processing.
 */
void TetrisX::trySolidize()
{
	if (!moveInfo.canMove)
	{
		currID = 0;																	// key control and autoDown are implicitly "disabled"
		hitGround = true;															// impact detected

		uint16_t bottomIdx = 1;														// to avoid division by zero (even though it is guaranteed to be populated)
		for (B_Cell& b : currBlock)
		{
			board[b.idx].solid = true;												// currBlock is registered as SOLID in the board
			bottomIdx = b.idx;														// since currBlock is sorted in ascending order => last B_cell.idx = max
		}
		bottomIdx /= Configs::COLS;													// bottommost row as the index, from which 4 rows upwards must be checked

		if (bottomIdx > 0)
		{
			bottomRow = bottomIdx;													// bottomRow is uint8_t, but indexing unfortunately requires values ​​> 255
			complFSM = State::BOOM_1;												// the completion routine begins
		}
	}
}

/**
 * @brief Stage 1 of the completion routine: Determines whether and how many rows have been completed. If applicable, the IDs of the affected block cells
 * are assigned code 8 (💥) so that with the next `drawBoard()` call, the first stage of completion is displayed accordingly. Additionally, the function
 * collects the indices of these block cells in `complVec` and initiates stage 2 of the completion routine for subsequent animation stages.
 * @param bottomRow the bottommost row where the `currBlock` encountered a fixed block from which the number of completed rows (max. 4) is to be checked upwards
 */
void TetrisX::completion_1(uint8_t bottomRow)
{
	complFSM = State::IDLE;															// assumption: No line was completed. Reset complFSM to IDLE.
	newBlock = true;																// assumption: a newBlock phase then begins
	complVec.clear();

	std::array<uint16_t, 12> line;													// buffer for candidate indices per row
	bool isContinous = true;														// continuity detection (line without a gap?)
	uint8_t cnt = 0;

	uint16_t endIdx = bottomRow * Configs::COLS + 13;								// -1 is performed directly on the element -> [--endIdx]
	uint16_t begIdx = (bottomRow - 3) * Configs::COLS;								// max. 4 lines are possible, but we index from the start of the line

	while (endIdx > begIdx)															// a maximum of 4 lines are checked
	{
		isContinous &= board[--endIdx].solid;
		line[cnt] = endIdx;

		if (++cnt == 12)
		{
			if (isContinous)														// if 12 elements of a row are SOLID => row completed
			{
				for (uint8_t i = 0; i < 12; ++i)
				{
					complVec.push_back(line[i]);									// store the indices of every cell in the entire row in complVec
					board[line[i]].id = 8;											// mark the block cells of the entire row with code 8 = 💥
				}
				newBlock = false;													// the next newBlock phase begins only after full completion.
				actionPerformed = true;
				complFSM = State::BOOM_2;											// next stage of the completion routine
			}
			cnt = 0; isContinous = true;											// reset the control elements
			endIdx -= 2;															// skip the left margin
		}
	}
}

/**
 * @brief Stage 2 of the completion routine: the IDs of the previously identified block cells are assigned the next emoji (Code 9 = 💢),
 * and the next stage of the routine is scheduled.
 */
void TetrisX::completion_2()
{
	for (uint16_t& idx : complVec)
		board[idx].id = 9;															// set the next VISUALS stage by ID (Code 9 = 💢)

	complFSM = State::DROP;															// next stage of the completion routine
}

/**
 * @brief The final stage of the completion routine: the cells marked as completed (`complVec`) are now replaced by the cells located above them,
 * offset by the determined number of rows. Here, too, the loop is exited early for efficiency reasons as soon as an empty row is encountered after
 * all lines to be deleted have been processed. Additionally, the new (lower) indices are set accordingly, and the original cells are marked as empty.
 * Thus, only the relevant stack above the completed area is correctly shifted downwards.
 */
void TetrisX::completion_3()
{
	uint8_t rows = complVec.size() / (Configs::COLS - 2);							// number of rows to delete, excluding the L/R margins
	uint16_t minIdx = rows * Configs::COLS;											// offset now adjusted to a full row of the board (including margins)

	score += Configs::SCORE_TABLE[rows - 1];
	lines += rows;
	line_cnt += rows;
	if (line_cnt > 9)
	{
		++level;
		line_cnt -= 10;
	}

	uint8_t cnt = 0, solids = 0, off = Configs::COLS;
	bool first = true;

	for (uint16_t i = complVec[0]; i > minIdx; --i)									// complVec[0]: lowest & last cell to be deleted
	{
		if (first)
		{
			while (board[i - off].id == 9)
				off += Configs::COLS;												// skip completed lines

			first = false;
		}

		uint16_t offIdx = i - off;

		board[i] = board[offIdx];													// shift the cell down by "off" rows
		board[i].idx = i;															// update Cell.idx to the new location
		board[offIdx] = { offIdx, 0, false };										// set source cell to empty = {offIdx, 0, false}

		if (board[i].solid) ++solids;												// count the SOLID cells

		if (++cnt == 12)															// line finished?
		{
			if (rows > 0) --rows;													// only after all lines to be deleted have been processed
			else if (solids == 0) break;											// check if a line without blocks was encountered => break

			first = true;															// check the offset again
			solids = 0; cnt = 0;													// reset counter after every full line
			i -= 2; 																// skip margins
		}
	}
}
