#ifndef MESSAGE_H
#define MESSAGE_H

class Message
{
public:
    enum { ResponseId = 1 };

    Message() {}
    virtual ~Message() {}
    virtual int messageId() const = 0;
};

#endif // MESSAGE_H
