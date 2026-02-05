import { Storage } from '@google-cloud/storage';
import { glob, globSync, globStream, globStreamSync, Glob } from 'glob';
import fs from 'fs';

async function main(sourceFile, destBucket, destPath) {
    console.log(`Uploading '${sourceFile}' to GCS at ${destBucket}:${destPath} ...`);
    try {
        const storage = new Storage({
            keyFilename: './key.json'
        });

        await storage.bucket(destBucket).upload(sourceFile, {
            destination: destPath
        });

        process.exit(0);
    } catch (e) {
        console.error('Error: ', e);
        process.exit(1);
    }
}

const [,,sourceFile, destBucket, destPath] = process.argv;

if (!destPath || !destBucket || !sourceFile) {
    console.error('Usage: node upload.js source-file dest-bucket dest-path');
    process.exit(1);
}

main(sourceFile, destBucket, destPath);
