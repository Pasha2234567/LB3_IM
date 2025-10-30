#include <iostream>
#include <iomanip>
#include <locale>
#include <vector>
#include <string>
#include <cmath>

using namespace std;

// === КОНСТАНТЫ МОДЕЛИ (можно менять, но они фиксированы в начале) ===
const double INIT_ACCOUNT = 10000.0;     // Начальный баланс на счёте
const double BASIC_PRICE = 30.0;         // Базовая цена для расчёта спроса
const double OFFER_BASE_PRICE = 35.0;    // Базовая цена мелкооптовой партии
const double MAX_DEMAND = 100.0;         // Максимальный спрос
const double MEAN_D_PRICE = 100.0;       // Средняя цена спроса
const double DAILY_SPENDING = 700.0;     // Ежедневные расходы
const double RENT_RATE = 200.0;          // Аренда
const double WAGES_AND_TAXES = 500.0;    // Зарплаты и налоги (в день)
const double VAT_RATE = 0.2;             // НДС 20%
const double OFFER_VOLUME_BASE = 40.0;   // Базовый объём мелкооптовой партии
const double OFFER_VOLUME_RND = 10.0;    // Случайное отклонение объёма
const double BASIC_STORE_INIT = 360.0;   // Начальный запас на базовом складе
const double SHOP_STORE_INIT = 80.0;     // Начальный запас в магазине
const int SIMULATION_DAYS = 100;         // Сколько дней моделируем

// === ПЕРЕМЕННЫЕ СОСТОЯНИЯ (изменяются во времени) ===
struct ModelState {
    double account;              // Счёт
    double basicStore;           // Запас на базовом складе
    double shopStore;            // Запас в магазине
    double income;               // Доход за день
    double demand;               // Спрос
    double retPrice;             // Цена продажи
    double offerVolume;          // Объём мелкооптовой партии
    double offerPrice;           // Цена за единицу в партии
    double offerAccept;          // Принята ли партия (0 или 1)
    double transferVol;          // Объём перевозки
    double sold;                 // Продано за день
    double lost;                 // Потерянный спрос
    double dailySpending;        // Расходы за день
    int day;                     // Текущий день

    // === НОВАЯ ФУНКЦИЯ: Поэтапная оплата ===
    double offerDebt;            // Долг по мелкооптовой партии
    int paymentInstallments;     // Сколько осталось платежей
    double installmentAmount;    // Сумма одного платежа
};

// === ФУНКЦИЯ: Вывод текущего состояния ===
void printState(const ModelState& state) {
    cout << "\n";
    cout << "========================================\n";
    cout << "           ДЕНЬ " << state.day << " из " << SIMULATION_DAYS << "\n";
    cout << "========================================\n";
    cout << fixed << setprecision(2);
    cout << "Состояние счёта:         " << state.account << " руб.\n";
    cout << "Запас на складе (база):  " << state.basicStore << " ед.\n";
    cout << "Запас в магазине:        " << state.shopStore << " ед.\n";
    cout << "Спрос сегодня:           " << state.demand << " ед.\n";
    cout << "Цена продажи:            " << state.retPrice << " руб./ед.\n";
    cout << "Доход за день:           " << state.income << " руб.\n";
    cout << "Продано:                 " << state.sold << " ед.\n";
    cout << "Потеряно (не хватило):   " << state.lost << " ед.\n";

    cout << "\n--- Мелкооптовое предложение ---\n";
    cout << "Объём партии:            " << state.offerVolume << " ед.\n";
    cout << "Цена за единицу:         " << state.offerPrice << " руб.\n";
    cout << "Общая стоимость:         " << (state.offerVolume * state.offerPrice) << " руб.\n";

    if (state.offerDebt > 0) {
        cout << "Долг по партии:          " << state.offerDebt << " руб.\n";
        cout << "Осталось платежей:       " << state.paymentInstallments << "\n";
        cout << "Платёж в рассрочку:      " << state.installmentAmount << " руб.\n";
    }
    else {
        cout << "Долга по партии нет.\n";
    }

    cout << "----------------------------------------\n";
}

// === ФУНКЦИЯ: Ввод данных пользователем ===
void inputStep(ModelState& state) {
    cout << "\nВВОД ДАННЫХ НА ДЕНЬ " << state.day << ":\n";
    cout << "----------------------------------------\n";

    // 1. Объём перевозки (0 = не перевозим)
    double transfer;
    cout << "Сколько товара перевезти с базы в магазин? (0 = ничего): ";
    cin >> transfer;
    state.transferVol = max(0.0, min(transfer, state.basicStore)); // не больше, чем есть

    // 2. Решение по мелкооптовой партии (0 или 1)
    int accept;
    cout << "Принять мелкооптовую партию? (1 = да, 0 = нет): ";
    cin >> accept;
    state.offerAccept = (accept == 1) ? 1.0 : 0.0;

    // Если принимаем — спросим, в рассрочку ли?
    if (state.offerAccept == 1.0 && state.offerDebt == 0) {
        int installments;
        cout << "Оплатить сразу или в рассрочку? (1 = сразу, 3 = 3 платежа, 6 = 6 платежей): ";
        cin >> installments;
        if (installments == 3 || installments == 6) {
            state.paymentInstallments = installments;
            double total = state.offerVolume * state.offerPrice;
            state.installmentAmount = total / installments;
            state.offerDebt = total - state.installmentAmount; // первый платёж сразу
            state.account -= state.installmentAmount;
            cout << "Оплачен первый платёж: " << state.installmentAmount << " руб.\n";
            cout << "Осталось долга: " << state.offerDebt << " руб. на " << (installments - 1) << " платежей.\n";
        }
        else {
            // Оплата сразу
            double total = state.offerVolume * state.offerPrice;
            if (state.account >= total) {
                state.account -= total;
                state.offerDebt = 0;
                state.paymentInstallments = 0;
            }
            else {
                cout << "Недостаточно денег! Партия отклонена.\n";
                state.offerAccept = 0.0;
            }
        }
    }

    // 3. Цена продажи
    cout << "Установить цену продажи (рекомендуется 15-25 руб.): ";
    cin >> state.retPrice;
    state.retPrice = max(10.0, min(state.retPrice, 50.0)); // ограничим
}

// === ОСНОВНАЯ СИМУЛЯЦИЯ ===
int main() {
    setlocale(LC_ALL, "Russian"); // Русский язык в консоли

    cout << "=======================================\n";
    cout << "   СИМУЛЯТОР МАГАЗИНА С РАССРОЧКОЙ\n";
    cout << "   (на основе Powersim Studio)\n";
    cout << "=======================================\n\n";

    ModelState state;
    // Инициализация
    state.account = INIT_ACCOUNT;
    state.basicStore = BASIC_STORE_INIT;
    state.shopStore = SHOP_STORE_INIT;
    state.income = 0;
    state.demand = 0;
    state.retPrice = 15.0;
    state.offerVolume = 0;
    state.offerPrice = 0;
    state.offerAccept = 0;
    state.transferVol = 0;
    state.sold = 0;
    state.lost = 0;
    state.dailySpending = DAILY_SPENDING;
    state.day = 0;
    state.offerDebt = 0;
    state.paymentInstallments = 0;
    state.installmentAmount = 0;

    // === Цикл по дням ===
    for (state.day = 1; state.day <= SIMULATION_DAYS; ++state.day) {

        // --- ШАГ 1: Генерация мелкооптового предложения (каждые 10 дней) ---
        if (state.day % 10 == 1) {
            state.offerVolume = OFFER_VOLUME_BASE + (rand() % 20 - 10); // 30–50
            state.offerVolume = max(30.0, min(state.offerVolume, 50.0));
            state.offerPrice = OFFER_BASE_PRICE + (rand() % 10 - 5); // 30–40
            state.offerPrice = max(30.0, min(state.offerPrice, 40.0));
        }

        // --- ШАГ 2: Вывод состояния и ввод данных ---
        printState(state);
        if (state.day < SIMULATION_DAYS) {
            inputStep(state);
        }

        // --- ШАГ 3: Перевозка товара ---
        if (state.transferVol > 0) {
            state.basicStore -= state.transferVol;
            state.shopStore += state.transferVol;
            cout << "Перевезено " << state.transferVol << " ед. с базы в магазин.\n";
        }

        // --- ШАГ 4: Поступление мелкооптовой партии (если принято) ---
        if (state.offerAccept == 1.0 && state.offerDebt == 0 && state.paymentInstallments == 0) {
            // Уже оплатили сразу выше
            state.basicStore += state.offerVolume;
            cout << "Получена партия: " << state.offerVolume << " ед.\n";
        }
        else if (state.offerAccept == 1.0 && state.paymentInstallments > 0) {
            // Уже обработано в inputStep (первый платёж)
            state.basicStore += state.offerVolume;
            cout << "Получена партия в рассрочку: " << state.offerVolume << " ед.\n";
        }

        // --- ШАГ 5: Расчёт спроса (зависит от цены) ---
        // Спрос = MAX_DEMAND * (MEAN_D_PRICE / retPrice)
        state.demand = MAX_DEMAND * (MEAN_D_PRICE / state.retPrice);
        state.demand = max(0.0, state.demand + (rand() % 40 - 20)); // шум

        // --- ШАГ 6: Продажа ---
        double canSell = min(state.shopStore, state.demand);
        state.sold = canSell;
        state.shopStore -= canSell;
        state.lost = state.demand - canSell;
        state.income = canSell * state.retPrice;

        // --- ШАГ 7: Учёт дохода ---
        state.account += state.income;

        // --- ШАГ 8: Ежедневные расходы ---
        state.account -= DAILY_SPENDING; // аренда, зарплаты и т.д.
        state.account -= RENT_RATE;
        state.account -= WAGES_AND_TAXES;

        // --- ШАГ 9: Оплата рассрочки (если есть) ---
        if (state.paymentInstallments > 1 && state.offerDebt > 0) {
            if (state.account >= state.installmentAmount) {
                state.account -= state.installmentAmount;
                state.offerDebt -= state.installmentAmount;
                state.paymentInstallments--;
                cout << "Оплачен платёж по рассрочке: " << state.installmentAmount << " руб.\n";
            }
            else {
                cout << "Не хватает денег на платёж по рассрочке!\n";
            }
        }
        else if (state.paymentInstallments == 1 && state.offerDebt > 0) {
            // Последний платёж
            if (state.account >= state.offerDebt) {
                state.account -= state.offerDebt;
                cout << "Оплачен последний платёж: " << state.offerDebt << " руб.\n";
                state.offerDebt = 0;
                state.paymentInstallments = 0;
            }
        }

        // --- ШАГ 10: Защита от отрицательного счёта ---
        if (state.account < 0) {
            cout << "ВНИМАНИЕ: Счёт ушёл в минус: " << state.account << " руб.\n";
        }

        // --- Пауза (чтобы пользователь видел) ---
        if (state.day < SIMULATION_DAYS) {
            cout << "\nНажмите Enter для продолжения на следующий день...\n";
            cin.ignore();
            cin.get();
        }
    }

    // === ИТОГ ===
    cout << "\n\n=======================================\n";
    cout << "       СИМУЛЯЦИЯ ЗАВЕРШЕНА\n";
    cout << "=======================================\n";
    cout << "Итоговый счёт: " << fixed << setprecision(2) << state.account << " руб.\n";
    cout << "Остаток на складе (база): " << state.basicStore << " ед.\n";
    cout << "Остаток в магазине: " << state.shopStore << " ед.\n";
    if (state.offerDebt > 0) {
        cout << "Осталось долга: " << state.offerDebt << " руб.\n";
    }
    else {
        cout << "Долгов нет.\n";
    }
    cout << "=======================================\n";

    return 0;
}
