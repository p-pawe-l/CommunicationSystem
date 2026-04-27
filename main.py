import drone_system


def main() -> None:
    system = drone_system.System()

    system.attachClient("controller-1")
    system.attachClient("agent-1")

    system.send({
        "sender": "agent-1",
        "receiver": "controller-1",
        "type": "ping",
        "data": {
            "x": 1,
            "message": "hello from Python",
        },
    })

    message = system.receive("controller-1")
    print(message)


if __name__ == "__main__":
    main()
