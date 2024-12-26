// script.js
document.addEventListener("DOMContentLoaded", () => {
    const createSessionButton = document.getElementById("create-session-button");
    const mySessionIdElement = document.getElementById("my-session-id");
    const joinSessionButton = document.getElementById("join-session-button");
    const joinSessionIdInput = document.getElementById("join-session-id");
    const joinStatusElement = document.getElementById("join-status");
    const gameContainer = document.getElementById("game-container");
    const statusElement = document.getElementById("status");
    const boardElement = document.getElementById("game-board");
    const newGameButton = document.getElementById("new-game");

    const ROWS = 6;
    const COLUMNS = 7;

    let currentSessionId = "";
    let playerNumber = 0; 
    let gameStatus = "ongoing";
    let board = [];
    let currentTurn = 1;
    let pollingInterval = null;

    // Создать новую сессию (player1)
    createSessionButton.addEventListener("click", async () => {
        try {
            console.log("Create session button clicked!");
            const response = await fetch("/create_session", {
                method: "POST"
            });
            if (!response.ok) {
                throw new Error("Failed to create session");
            }
            const data = await response.json();
            currentSessionId = data.session_id;
            playerNumber = data.player; // =1
            mySessionIdElement.textContent =
                `Ваш ID сессии: ${currentSessionId} (Игрок ${playerNumber})`;
            gameContainer.style.display = "block";
            joinStatusElement.textContent = "";
            await loadState();
            startPolling();
        } catch (error) {
            console.error(error);
            joinStatusElement.textContent = "Ошибка при создании сессии.";
        }
    });

    // Присоединиться к существующей сессии (player2)
    joinSessionButton.addEventListener("click", async () => {
        const sessionId = joinSessionIdInput.value.trim();
        console.log("Join session button clicked!", sessionId);
        if (sessionId === "") {
            joinStatusElement.textContent = "Введите ID сессии.";
            return;
        }
        try {
            const response = await fetch("/join_session", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: `session_id=${encodeURIComponent(sessionId)}`
            });
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(errorText);
            }
            const data = await response.json();
            currentSessionId = sessionId;
            playerNumber = 2;
            mySessionIdElement.textContent =
                `Вы присоединились к сессии: ${currentSessionId} (Игрок ${playerNumber})`;
            gameContainer.style.display = "block";
            joinStatusElement.textContent = "";
            await loadState();
            startPolling();
        } catch (error) {
            console.error(error);
            joinStatusElement.textContent = `Ошибка: ${error.message}`;
        }
    });

    // Загрузка состояния игры
    async function loadState() {
        try {
            const response = await fetch(
                `/get_state?session_id=${encodeURIComponent(currentSessionId)}`
            );
            if (!response.ok) {
                throw new Error("Failed to fetch game state");
            }
            const data = await response.json();
            gameStatus = data.status;
            board = data.board;
            currentTurn = data.current_turn;
            renderBoard();
            updateStatus();
        } catch (error) {
            console.error(error);
            statusElement.textContent = "Ошибка загрузки состояния игры.";
        }
    }

    function renderBoard() {
        boardElement.innerHTML = "";
        for(let i = 0; i < ROWS; i++) {
            for(let j = 0; j < COLUMNS; j++) {
                const cell = document.createElement("div");
                cell.classList.add("cell");
                if(board[i][j] === "X") {
                    const piece = document.createElement("div");
                    piece.classList.add("piece","user");
                    cell.appendChild(piece);
                }
                else if(board[i][j] === "O") {
                    const piece = document.createElement("div");
                    piece.classList.add("piece","server");
                    cell.appendChild(piece);
                }
                else {
                    cell.classList.add("empty");
                    cell.addEventListener("click", () => makeMove(j));
                }
                boardElement.appendChild(cell);
            }
        }
    }

    function updateStatus() {
        if(gameStatus === "ongoing") {
            if(currentTurn === playerNumber) {
                statusElement.textContent = "Ваш ход";
            } else {
                statusElement.textContent = "Ход соперника";
            }
            newGameButton.style.display = "none";
        }
        else if(gameStatus === "win_user") {
            if(playerNumber === 1) {
                statusElement.textContent = "Поздравляем! Вы (player1) выиграли!";
            } else {
                statusElement.textContent = "Соперник (player1) выиграл!";
            }
            newGameButton.style.display = "inline-block";
        }
        else if(gameStatus === "win_server") {
            if(playerNumber === 2) {
                statusElement.textContent = "Поздравляем! Вы (player2) выиграли!";
            } else {
                statusElement.textContent = "Соперник (player2) выиграл!";
            }
            newGameButton.style.display = "inline-block";
        }
        else if(gameStatus === "draw") {
            statusElement.textContent = "Ничья!";
            newGameButton.style.display = "inline-block";
        }
    }

    // Сделать ход
    async function makeMove(column) {
        if(gameStatus !== "ongoing") {
            alert("Игра завершена. Начните новую игру.");
            return;
        }
        if(currentTurn !== playerNumber) {
            alert("Сейчас не ваш ход.");
            return;
        }
        try {
            const response = await fetch("/make_move", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: `session_id=${encodeURIComponent(currentSessionId)}&column=${column}&player=${playerNumber}`
            });
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(errorText);
            }
            const data = await response.json();
            board = data.board;
            gameStatus = data.status;
            currentTurn = data.current_turn;
            renderBoard();
            updateStatus();
        } catch (error) {
            console.error(error);
            statusElement.textContent = "Ошибка выполнения хода.";
        }
    }

    // Новая игра (сброс) в текущей сессии
    newGameButton.addEventListener("click", async () => {
        try {
            const response = await fetch("/reset_game", {
                method: "POST",
                headers: {
                    "Content-Type":"application/x-www-form-urlencoded"
                },
                body: `session_id=${encodeURIComponent(currentSessionId)}`
            });
            if(!response.ok){
                const errorText=await response.text();
                throw new Error(errorText);
            }
            await loadState();
        } catch(error){
            console.error(error);
            statusElement.textContent="Ошибка при сбросе игры.";
        }
    });

    // Периодический опрос состояния (каждые 2 сек)
    function startPolling() {
        if(pollingInterval) {
            clearInterval(pollingInterval);
        }
        pollingInterval=setInterval(loadState,2000);
    }

    function initBoard() {
        boardElement.innerHTML="";
    }

    initBoard();
});
