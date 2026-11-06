# Private download website

Charter's download website and update API run on a private Node.js server.
The owner provides its HTTPS address and credentials to authorized users.
Because access is restricted, this public repository does not publish the
server address or its source code.

The website lists available platform builds. A future updater can read
`/api/v1/releases/{platform}/latest` with its own configured credentials,
download the listed file, and check its SHA-256 digest. Credentials must not
be compiled into a public app. The Windows application does not yet have an
automatic update client.
