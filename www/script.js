// www/script.js
document.addEventListener('DOMContentLoaded', () => {
    const board = document.getElementById('game-board');
    const status = document.getElementById('current-player');

    // Создание игрового поля
    for(let i = 0; i < 6; i++) {
        for(let j = 0; j < 7; j++) {
            const cell = document.createElement('div');
            cell.classList.add('cell');
            cell.dataset.column = j;
            board.appendChild(cell);
        }
    }

    // Функция для обновления игрового поля
    function updateBoard(boardState) {
        const cells = document.querySelectorAll('.cell');
        cells.forEach(cell => {
            const row = Math.floor(Array.from(cells).indexOf(cell) / 7);
            const col = cell.dataset.column;
            const cellValue = boardState[row][col];
            cell.classList.remove('player1', 'player2');
            if(cellValue === 'X') {
                cell.classList.add('player1');
            }
            else if(cellValue === 'O') {
                cell.classList.add('player2');
            }
        });
    }

    // Функция для получения состояния игры
    function getState() {
        fetch('/state')
            .then(response => response.json())
            .then(data => {
                updateBoard(data.board);
                status.textContent = data.current_player;
            })
            .catch(error => console.error('Error:', error));
    }

    // Инициальное получение состояния игры
    getState();

    // Обработчик кликов по клеткам
    board.addEventListener('click', (e) => {
        if(e.target.classList.contains('cell')) {
            const column = e.target.dataset.column;
            // Отправка хода на сервер
            fetch('/move', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: column
            })
            .then(response => response.json())
            .then(data => {
                if(data.error) {
                    alert(data.error);
                } else {
                    updateBoard(data.board);
                    status.textContent = data.current_player;
                }
            })
            .catch(error => console.error('Error:', error));
        }
    });

    // Автоматическое обновление состояния каждые 2 секунды
    setInterval(getState, 2000);
});
