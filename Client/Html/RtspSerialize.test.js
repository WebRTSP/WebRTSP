"use strict"

const assert = require('assert');

suite("RTSP Request serialize", () => {
    var Parse = null;
    var Serialize = null;

    setup(async () => {
         Parse = await import("./RtspParse.mjs");
         Serialize = await import("./RtspSerialize.mjs");
    });

    test("OPTIONS request", () => {
        const buffer =
            "OPTIONS * WEBRTSP/0.1\r\n" +
            "CSeq: 1\r\n";
        let request = Parse.ParseRequest(buffer);
        let serialized = Serialize.SerializeRequest(request);
        assert.strictEqual(buffer, serialized);
    });
});

suite("RTSP Response serialize", () => {
    var Parse = null;
    var Serialize = null;

    setup(async () => {
         Parse = await import("./RtspParse.mjs");
         Serialize = await import("./RtspSerialize.mjs");
    });

    test("Minimal response", () => {
        const buffer =
            "WEBRTSP/0.1 200 OK\r\n" +
            "CSeq: 9\r\n"
        let response = Parse.ParseResponse(buffer);
        let serialized = Serialize.SerializeResponse(response);
        assert.strictEqual(buffer, serialized);
    });

});