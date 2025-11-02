let cookies = 0;
let autoClickers = 0;
let autoCost = 10;

const cookie = document.getElementById('cookie');
const countEl = document.getElementById('count');
const buyAutoBtn = document.getElementById('buyAuto');
const autoCountEl = document.getElementById('autoCount');
const autoCostEl = document.getElementById('autoCost');

cookie.addEventListener('click', () => {
  cookies++;
  updateDisplay();
});

buyAutoBtn.addEventListener('click', () => {
  if (cookies >= autoCost) {
    cookies -= autoCost;
    autoClickers++;
    autoCost = Math.floor(autoCost * 1.5);
    updateDisplay();
  }
});

setInterval(() => {
  cookies += autoClickers;
  updateDisplay();
}, 1000);

function updateDisplay() {
  countEl.textContent = cookies;
  autoCountEl.textContent = autoClickers;
  autoCostEl.textContent = autoCost;
}
