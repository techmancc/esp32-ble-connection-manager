
Running the Config Manager (Client form)

Use the root folder, not the client folder.

Your client app is now running at:

http://localhost:5173/
Network URL: http://10.0.0.8:5173/
Why this matters:

This project’s Vite config sets root to client internally in vite.config.ts:29, and npm scripts are in package.json:6.
There is no separate client/package.json, so cd client then npm run dev will not work.
Exact commands to run next time:

cd C:\Users\carlsc12\esp32-ble-connection-manager
npm install (only if needed)
npm run dev
Open http://localhost:5173/


***************************************

npm run dev:host:clean