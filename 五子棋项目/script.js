document.addEventListener('DOMContentLoaded', () => {
    const BOARD_SIZE = 15;
    const EMPTY = 0;
    const BLACK = 1;
    const WHITE = 2;
    
    let board = [];
    let currentPlayer = BLACK;
    let gameActive = true;
    let lastMove = null;
    
    const boardElement = document.getElementById('board');
    const turnText = document.getElementById('turn-text');
    const stonePreview = document.getElementById('stone-preview');
    const resetBtn = document.getElementById('reset-btn');
    const modalResetBtn = document.getElementById('modal-reset-btn');
    const modal = document.getElementById('modal');
    const winnerText = document.getElementById('winner-text');
    
    // Star points for a standard 15x15 board
    const starPoints = [
        [3, 3], [3, 11],
        [7, 7],
        [11, 3], [11, 11]
    ];
    
    function initGame() {
        board = Array(BOARD_SIZE).fill(null).map(() => Array(BOARD_SIZE).fill(EMPTY));
        currentPlayer = BLACK;
        gameActive = true;
        lastMove = null;
        updateTurnIndicator();
        renderBoard();
        hideModal();
    }
    
    function isStarPoint(row, col) {
        return starPoints.some(point => point[0] === row && point[1] === col);
    }
    
    function renderBoard() {
        boardElement.innerHTML = '';
        for (let row = 0; row < BOARD_SIZE; row++) {
            for (let col = 0; col < BOARD_SIZE; col++) {
                const cell = document.createElement('div');
                cell.className = 'cell';
                cell.dataset.row = row;
                cell.dataset.col = col;
                
                if (isStarPoint(row, col)) {
                    cell.classList.add('star-point');
                }
                
                // Add ghost stone for hover effect
                const ghost = document.createElement('div');
                ghost.className = 'ghost-stone';
                cell.appendChild(ghost);
                
                cell.addEventListener('click', () => handleCellClick(row, col));
                
                // Update ghost stone color based on current player
                cell.addEventListener('mouseenter', () => {
                    if (gameActive && board[row][col] === EMPTY) {
                        ghost.className = `ghost-stone ${currentPlayer === BLACK ? 'black' : 'white'}`;
                    }
                });
                
                boardElement.appendChild(cell);
            }
        }
    }
    
    function handleCellClick(row, col) {
        if (!gameActive || board[row][col] !== EMPTY) return;
        
        board[row][col] = currentPlayer;
        placeStone(row, col);
        
        // Remove 'latest' class from previous move
        if (lastMove) {
            const prevStone = document.querySelector(`.stone[data-row="${lastMove.row}"][data-col="${lastMove.col}"]`);
            if (prevStone) prevStone.classList.remove('latest');
        }
        
        // Add 'latest' class to current move
        const currentStone = document.querySelector(`.stone[data-row="${row}"][data-col="${col}"]`);
        if (currentStone) currentStone.classList.add('latest');
        
        lastMove = { row, col };
        
        const winningLine = checkWin(row, col);
        if (winningLine) {
            handleWin(winningLine);
        } else if (checkDraw()) {
            handleDraw();
        } else {
            currentPlayer = currentPlayer === BLACK ? WHITE : BLACK;
            updateTurnIndicator();
        }
    }
    
    function placeStone(row, col) {
        const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"]`);
        const stone = document.createElement('div');
        stone.className = `stone ${currentPlayer === BLACK ? 'black' : 'white'}`;
        stone.dataset.row = row;
        stone.dataset.col = col;
        cell.appendChild(stone);
        
        // Hide ghost stone
        const ghost = cell.querySelector('.ghost-stone');
        if (ghost) ghost.style.display = 'none';
    }
    
    function updateTurnIndicator() {
        if (currentPlayer === BLACK) {
            turnText.textContent = '黑方回合';
            stonePreview.className = 'stone-preview black';
        } else {
            turnText.textContent = '白方回合';
            stonePreview.className = 'stone-preview white';
        }
    }
    
    function checkWin(row, col) {
        const directions = [
            [[0, 1], [0, -1]], // Horizontal
            [[1, 0], [-1, 0]], // Vertical
            [[1, 1], [-1, -1]], // Diagonal \
            [[1, -1], [-1, 1]]  // Diagonal /
        ];
        
        const player = board[row][col];
        
        for (const dir of directions) {
            let count = 1;
            let line = [{row, col}];
            
            // Check both ways in a direction
            for (const [dRow, dCol] of dir) {
                let r = row + dRow;
                let c = col + dCol;
                while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] === player) {
                    count++;
                    line.push({row: r, col: c});
                    r += dRow;
                    c += dCol;
                }
            }
            
            if (count >= 5) {
                return line;
            }
        }
        return null;
    }
    
    function checkDraw() {
        for (let row = 0; row < BOARD_SIZE; row++) {
            for (let col = 0; col < BOARD_SIZE; col++) {
                if (board[row][col] === EMPTY) return false;
            }
        }
        return true;
    }
    
    function handleWin(winningLine) {
        gameActive = false;
        
        // Highlight winning stones
        winningLine.forEach(({row, col}) => {
            const stone = document.querySelector(`.stone[data-row="${row}"][data-col="${col}"]`);
            if (stone) stone.classList.add('win-highlight');
        });
        
        setTimeout(() => {
            winnerText.textContent = currentPlayer === BLACK ? '黑方获胜！' : '白方获胜！';
            modal.classList.remove('hidden');
        }, 800);
    }
    
    function handleDraw() {
        gameActive = false;
        setTimeout(() => {
            winnerText.textContent = '平局！';
            modal.classList.remove('hidden');
        }, 500);
    }
    
    function hideModal() {
        modal.classList.add('hidden');
    }
    
    resetBtn.addEventListener('click', initGame);
    modalResetBtn.addEventListener('click', initGame);
    
    // Initialize
    initGame();
});
