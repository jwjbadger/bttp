let counter = 0;

const counterDsp = document.getElementById('count');
const incrementBtn = document.getElementById('incr');
const decrementBtn = document.getElementById('decr');
const resetBtn = document.getElementById('reset');

incrementBtn.addEventListener('click', () => {
    counter++;
    counterDsp.innerHTML = counter;
});

decrementBtn.addEventListener('click', () => {
    counter--;
    counterDsp.innerHTML = counter;
});

resetBtn.addEventListener('click', () => {
    counter = 0;
    counterDsp.innerHTML = counter;
});