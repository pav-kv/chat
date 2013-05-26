#pragma once

#include "common.h"

#include <string>
#include <vector>

#include <unistd.h>

using std::vector;
using std::string;

const uint32_t MSG_UNKNOWN  = 0;
const uint32_t MSG_SIGN_IN  = 1;
const uint32_t MSG_SIGN_OUT = 2;
const uint32_t MSG_DENY     = 3;
const uint32_t MSG_LIST     = 4;
const uint32_t MSG_TEXT     = 5;
const uint32_t MSG_ACK      = 6;
const uint32_t MSG_GETTEXT  = 7;
const uint32_t MSG_TEXTFIN  = 8;

void PushToBuffer(const char* from, size_t size, vector<char>& buffer) {
    const char* byte = from;
    for (size_t i = 0; i < size; ++i)
        buffer.push_back(*byte++);
}

void PushToBuffer(const string& str, vector<char>& buffer) {
    uint32_t size = str.size();
    PushToBuffer((const char*)&size, sizeof(size), buffer);
    PushToBuffer(&str[0], size, buffer);
}

string PopFromBuffer(const vector<char>& buffer, size_t& pos) {
    uint32_t size = *((uint32_t*)&buffer[pos]);
    pos += sizeof(size);
    string result(&buffer[pos], &buffer[pos] + size);
    pos += size;
    return result;
}

struct TMessageHeader {
    uint32_t Tag;
    uint32_t Id;
    uint32_t Size;

    TMessageHeader(int tag = MSG_UNKNOWN, int id = 0)
        : Tag(tag)
        , Id(id)
        , Size(0)
    {
    }

    bool Write(int socketFD) const {
        vector<char> buffer;
        PushToBuffer((const char*)&Tag, sizeof(Tag), buffer);
        PushToBuffer((const char*)&Id, sizeof(Id), buffer);
        PushToBuffer((const char*)&Size, sizeof(Size), buffer);
        return (size_t)write(socketFD, &buffer[0], buffer.size()) == buffer.size();
    }

    bool Read(int socketFD) {
        vector<char> buffer(sizeof(Tag) + sizeof(Id) + sizeof(Size));
        if ((size_t)read(socketFD, &buffer[0], buffer.size()) != buffer.size())
            return false;
        Tag = *((int *)&buffer[0]);
        Id = *((int *)&buffer[sizeof(Tag)]);
        Size = *((uint32_t*)&buffer[sizeof(Tag) + sizeof(Id)]);
        return true;
    }
};

struct TMessage {
    TMessageHeader Header;

    TMessage(const TMessageHeader& header = TMessageHeader())
        : Header(header)
    {
    }

    virtual bool Write(int socketFD) {
        return Header.Write(socketFD);
    }

    virtual bool Read(const TMessageHeader& header, int socketFD) {
        Header = header;
        return true;
    }

    virtual ~TMessage() { }
};

struct TMessageSignIn : public TMessage {
    string Login;
    string Password;

    TMessageSignIn(const string& login = "", const string& password = "")
        : TMessage(MSG_SIGN_IN)
        , Login(login)
        , Password(password)
    {
    }

    virtual bool Write(int socketFD) {
        vector<char> buffer;
        PushToBuffer(Login, buffer);
        PushToBuffer(Password, buffer);
        Header.Size = buffer.size();
        if (!TMessage::Write(socketFD))
            return false;
        return (size_t)write(socketFD, &buffer[0], buffer.size()) == buffer.size();
    }

    virtual bool Read(const TMessageHeader& header, int socketFD) {
        Header = header;
        vector<char> buffer(Header.Size);
        if ((size_t)read(socketFD, &buffer[0], buffer.size()) != buffer.size())
            return false;
        size_t pos = 0;
        Login = PopFromBuffer(buffer, pos);
        Password = PopFromBuffer(buffer, pos);
        return true;
    }
};

struct TMessageText : public TMessage {
    string Sender;
    string Reciever;
    string Text;

    TMessageText()
        : TMessage(MSG_TEXT)
    {
    }

    virtual bool Write(int socketFD) {
        vector<char> buffer;
        PushToBuffer(Sender, buffer);
        PushToBuffer(Reciever, buffer);
        PushToBuffer(Text, buffer);
        Header.Size = buffer.size();
        if (!TMessage::Write(socketFD))
            return false;
        return (size_t)write(socketFD, &buffer[0], buffer.size()) == buffer.size();
    }

    virtual bool Read(const TMessageHeader& header, int socketFD) {
        Header = header;
        vector<char> buffer(Header.Size);
        if ((size_t)read(socketFD, &buffer[0], buffer.size()) != buffer.size())
            return false;
        size_t pos = 0;
        Sender = PopFromBuffer(buffer, pos);
        Reciever = PopFromBuffer(buffer, pos);
        Text = PopFromBuffer(buffer, pos);
        return true;
    }
};

struct TMessageList : public TMessage {
    vector<string> Users;

    TMessageList()
        : TMessage(MSG_LIST)
    {
    }

    virtual bool Write(int socketFD) {
        vector<char> buffer;
        uint32_t size = Users.size();
        PushToBuffer((const char*)&size, sizeof(size), buffer);
        for (size_t i = 0; i < size; ++i)
            PushToBuffer(Users[i], buffer);
        Header.Size = buffer.size();
        if (!TMessage::Write(socketFD))
            return false;
        return (size_t)write(socketFD, &buffer[0], buffer.size()) == buffer.size();
    }

    virtual bool Read(const TMessageHeader& header, int socketFD) {
        Header = header;
        vector<char> buffer(Header.Size);
        if ((size_t)read(socketFD, &buffer[0], buffer.size()) != buffer.size())
            return false;
        size_t pos = 0;
        uint32_t size = *((uint32_t*)&buffer[pos]);
        pos += sizeof(size);
        Users.resize(size);
        for (size_t i = 0; i < size; ++i)
            Users[i] = PopFromBuffer(buffer, pos);
        return true;
    }
};


TMessage* PopMessage(int connectionFD) {
    TMessageHeader header;
    if (!header.Read(connectionFD))
        return NULL;

    TMessage* msg = NULL;
    switch (header.Tag) {
    case MSG_SIGN_IN:
        msg = new TMessageSignIn();
        break;

    case MSG_SIGN_OUT: case MSG_ACK: case MSG_DENY: case MSG_GETTEXT: case MSG_TEXTFIN:
        msg = new TMessage(header.Tag);
        break;

    case MSG_TEXT:
        msg = new TMessageText();
        break;

    case MSG_LIST:
        msg = new TMessageList();
        break;
    }
    if (msg && !msg->Read(header, connectionFD))
        return NULL;

    return msg;
}

