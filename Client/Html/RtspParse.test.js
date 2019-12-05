"use strict"

const assert = require('assert');

suite("RTSP Response parse", () => {
    var Parse = null;
    var Method = null;
    var Protocol = null;

    setup(async () => {
         Parse = await import("./RtspParse.mjs");
         Method = await import("./RtspMethod.mjs");
         Protocol = await import("./RtspProtocol.mjs");
    });

    test("OPTIONS request", () => {
        let buffer =
            "OPTIONS * WEBRTSP/0.1\r\n" +
            "CSeq: 1\r\n";
        let request = Parse.ParseRequest(buffer);
        assert.ok(request);
        assert.ok(request.method === Method.OPTIONS);
        assert.ok(request.uri == "*");
        assert.ok(request.protocol === Protocol.WEBRTSP_0_1);
        assert.ok(request.cseq === 1);
        assert.ok(request.headerFields.size == 0);
        assert.ok(!request.body);
    });

    test("SETUP request", () => {
        let buffer =
            "SETUP rtsp://example.com/meida.ogg WEBRTSP/0.1\r\n" +
            "CSeq: 3\r\n" +
            "Content-Type: application/sdp\r\n" +
            "\r\n" +
            "<sdp>\r\n";
        let request = Parse.ParseRequest(buffer);
        assert.ok(request);
        assert.ok(request.method === Method.SETUP);
        assert.ok(request.uri == "rtsp://example.com/meida.ogg");
        assert.ok(request.protocol === Protocol.WEBRTSP_0_1);
        assert.ok(request.cseq === 3);
        assert.ok(request.headerFields.size == 1);
        assert.ok(request.headerFields.has("content-type"));
        assert.strictEqual(request.headerFields.get("content-type"), "application/sdp")
        assert.strictEqual(request.body, "<sdp>\r\n")
    });
});

suite("RTSP Response parse", () => {
    var Parse = null;
    var Method = null;
    var Protocol = null;

    setup(async () => {
         Parse = await import("./RtspParse.mjs");
         Method = await import("./RtspMethod.mjs");
         Protocol = await import("./RtspProtocol.mjs");
    });

    test("Minimal response", () => {
        let buffer =
            "WEBRTSP/0.1 200 OK\r\n" +
            "CSeq: 9\r\n"
        let response = Parse.ParseResponse(buffer);
        assert.ok(response);
        assert.ok(response.protocol === Protocol.WEBRTSP_0_1);
        assert.ok(response.statusCode === 200);
        assert.ok(response.reasonPhrase === "OK");
        assert.ok(response.cseq === 9);
        assert.ok(response.headerFields.size == 0);
        assert.ok(!response.body);
    });

    test("Response with header field", () => {
        let buffer =
            "WEBRTSP/0.1 200 OK\r\n" +
            "CSeq: 9\r\n" +
            "Content-Type: text/parameters\r\n"
        let response = Parse.ParseResponse(buffer);
        assert.ok(response);
        assert.ok(response.protocol === Protocol.WEBRTSP_0_1);
        assert.ok(response.statusCode === 200);
        assert.ok(response.reasonPhrase === "OK");
        assert.ok(response.cseq === 9);
        assert.ok(response.headerFields.size == 1);
        assert.ok(response.headerFields.has("content-type"));
        assert.strictEqual(response.headerFields.get("content-type"), "text/parameters")
        assert.ok(!response.body);
    });
});
