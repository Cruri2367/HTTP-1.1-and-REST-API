# Схема CRUD для ресурса /users

| Операция | Метод | URL | Тело запроса | Ожидаемый статус |
|---|---|---|---|---|
| Получить список | GET | /users | нет | 200 OK |
| Получить пользователя | GET | /users/42 | нет | 200 OK; 404 если нет |
| Создать пользователя | POST | /users | `{"name":"Ada","email":"ada@example.com"}` | 201 Created; 400 при неверном JSON |
| Полностью изменить | PUT | /users/42 | `{"name":"Ada Lovelace","email":"ada@example.com"}` | 200 OK или 204 No Content; 404 если нет |
| Частично изменить | PATCH | /users/42 | `{"name":"Ada"}` | 200 OK или 204 No Content; 404 если нет |
| Удалить | DELETE | /users/42 | нет | 204 No Content; 404 если нет |

## Схема вызовов

Клиент → `GET /users` → сервер → `200` + JSON-список

Клиент → `POST /users` + JSON → сервер → `201` + `Location`

Клиент → `PUT /users/42` + JSON → сервер → `200` или `204`

Клиент → `DELETE /users/42` → сервер → `204`
