import requests

BASE_URL = "https://jsonplaceholder.typicode.com"
TIMEOUT = 5

def show_response(label, response):
    print(f"\n{label}: HTTP {response.status_code}")
    if response.content:
        try:
            print(response.json())
        except ValueError:
            print(response.text[:500])

def main():
    r = requests.get(f"{BASE_URL}/users", timeout=TIMEOUT)
    r.raise_for_status()
    users = r.json()
    print("GET /users — первые три пользователя:")
    for user in users[:3]:
        print(f"id={user['id']}, name={user['name']}, email={user['email']}")

    r = requests.post(
        f"{BASE_URL}/users",
        json={"name": "Student", "username": "student", "email": "student@example.com"},
        timeout=TIMEOUT,
    )
    show_response("POST /users", r)

    r = requests.put(
        f"{BASE_URL}/users/1",
        json={"name": "Updated Student", "username": "updated"},
        timeout=TIMEOUT,
    )
    show_response("PUT /users/1", r)

    r = requests.delete(f"{BASE_URL}/users/1", timeout=TIMEOUT)
    show_response("DELETE /users/1", r)

if __name__ == "__main__":
    main()
