# CLIENT_WEB FRONT SOURCES

TypeScript source for the web client's front-end.

*tsc* (via *npm run build*, see *package.json*) compiles these into *.js*
files inside *../web/src/*, mirroring this folder's structure. *../web/src/*
is entirely generated - nothing there should be hand-edited, and it is not
committed to git.

*deploy_web_client.bat* runs the TypeScript build before deploying, so
*../web/* only ever needs to contain deployable static resources plus the
compiled output.
